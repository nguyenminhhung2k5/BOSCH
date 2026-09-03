"""Threaded, UI-independent serial communication service."""

from __future__ import annotations

import logging
import threading
from collections.abc import Callable
from typing import Protocol

import serial

from ..config.constants import (
    DATA_FORMATS,
    SERIAL_READ_TIMEOUT_SECONDS,
    SERIAL_WRITE_TIMEOUT_SECONDS,
)
from .data_codec import parse_hex_payload
from .exceptions import (
    EmptyCommandError,
    InvalidPayloadError,
    NotConnectedError,
    SerialConnectionError,
    SerialWriteError,
)
from .framing import frame_payload, remove_frame_markers


class SerialConnection(Protocol):
    """Small protocol implemented by pyserial and test doubles."""

    @property
    def is_open(self) -> bool:
        ...

    def close(self) -> None:
        ...

    def flush(self) -> None:
        ...

    def readline(self) -> bytes:
        ...

    def write(self, data: bytes) -> int:
        ...


SerialFactory = Callable[..., SerialConnection]
ConnectionLostCallback = Callable[[], None]


class SerialManager:
    """Own a serial connection and read incoming data without blocking the UI.

    The manager deliberately has no Tkinter dependency.  It can therefore be
    tested independently and reused by another user interface in the future.
    Log records are emitted through the logger supplied by the caller.
    """

    def __init__(
        self,
        logger: logging.Logger,
        *,
        serial_factory: SerialFactory | None = None,
        on_connection_lost: ConnectionLostCallback | None = None,
    ) -> None:
        self._logger = logger
        self._serial_factory = serial_factory or serial.Serial
        self._on_connection_lost = on_connection_lost
        self._connection: SerialConnection | None = None
        self._reader_thread: threading.Thread | None = None
        self._stop_reader = threading.Event()
        self._connection_lock = threading.RLock()

    @property
    def is_connected(self) -> bool:
        """Return whether the managed serial connection is open."""
        with self._connection_lock:
            connection = self._connection
            return connection is not None and connection.is_open

    def connect(self, port: str, baud_rate: int) -> None:
        """Open ``port`` and start the background reader thread."""
        if not port:
            raise SerialConnectionError("Select a COM port before connecting.")
        if baud_rate <= 0:
            raise SerialConnectionError("Baud rate must be greater than zero.")
        if self.is_connected:
            raise SerialConnectionError("A serial connection is already open.")

        try:
            connection = self._serial_factory(
                port=port,
                baudrate=baud_rate,
                timeout=SERIAL_READ_TIMEOUT_SECONDS,
                write_timeout=SERIAL_WRITE_TIMEOUT_SECONDS,
            )
        except (serial.SerialException, OSError, ValueError) as exc:
            raise SerialConnectionError(f"Could not open {port}: {exc}") from exc

        with self._connection_lock:
            self._connection = connection
            self._stop_reader.clear()
            self._reader_thread = threading.Thread(
                target=self._read_loop,
                args=(connection,),
                daemon=True,
                name="bea-serial-reader",
            )
            try:
                self._reader_thread.start()
            except RuntimeError as exc:
                self._connection = None
                self._reader_thread = None
                connection.close()
                raise SerialConnectionError(
                    f"Could not start the serial reader: {exc}"
                ) from exc

        self._logger.info("Connected to %s at %d baud.", port, baud_rate)

    def disconnect(self, *, announce: bool = True) -> None:
        """Stop the reader, close the port, and release all connection state."""
        with self._connection_lock:
            connection = self._connection
            reader_thread = self._reader_thread
            was_connected = connection is not None
            self._connection = None
            self._reader_thread = None
            self._stop_reader.set()

        if connection is not None:
            try:
                if connection.is_open:
                    connection.close()
            except (serial.SerialException, OSError) as exc:
                self._logger.warning("Error while closing the serial port: %s", exc)

        if (
            reader_thread is not None
            and reader_thread.is_alive()
            and reader_thread is not threading.current_thread()
        ):
            reader_thread.join(timeout=1.0)

        if announce and was_connected:
            self._logger.info("Disconnected.")

    def send_text(
        self,
        input_data: str,
        line_ending: str = "",
        *,
        framed: bool = True,
    ) -> bytes:
        """Encode and send a String command, returning wire bytes.

        ``input_data`` is kept unchanged; the selected line ending is appended
        before the BEA frame markers.  The default wire layout is:

        ``SOF + UTF-8(input_data + line_ending) + EOF``

        ``framed=False`` is available for a future transport-specific use
        case; normal application and diagnostic traffic stays framed.
        """
        if not input_data.strip():
            raise EmptyCommandError("Enter a command before sending.")

        payload = (input_data + line_ending).encode("utf-8")
        wire_payload = self._prepare_payload(payload, framed=framed)
        self._send_payload(wire_payload)
        visible_command = input_data.replace("\r", "\\r").replace("\n", "\\n")
        self._logger.info(
            "TX: [String] %s | HEX: %s",
            visible_command,
            payload.hex(" ").upper(),
        )
        return wire_payload

    def send_hex(self, input_data: str) -> bytes:
        """Parse hexadecimal text and send it as one framed payload."""
        return self.send_bytes(parse_hex_payload(input_data))

    def send_service_payload(self, payload: bytes, data_format: str) -> bytes:
        """Send a generated diagnostic payload in the selected data format.

        In ``Hex`` mode, ``payload`` is transmitted as its original binary
        bytes.  In ``String`` mode, the same payload is converted to an
        uppercase hexadecimal string, for example ``b"\\x22\\xF1\\x80"``
        becomes the ASCII text ``"22F180"``.  Both modes use the normal BEA
        frame markers, while the markers remain hidden from the activity log.
        """
        if not isinstance(data_format, str):
            raise InvalidPayloadError(
                f"Data format must be one of {', '.join(DATA_FORMATS)}."
            )

        normalized_format = data_format.strip().lower()
        if normalized_format == "hex":
            return self.send_bytes(payload)
        if normalized_format == "string":
            return self.send_text(bytes(payload).hex().upper())
        raise InvalidPayloadError(
            f"Data format must be one of {', '.join(DATA_FORMATS)}."
        )

    def send_bytes(
        self,
        payload: bytes | bytearray | memoryview,
        *,
        framed: bool = True,
    ) -> bytes:
        """Write raw bytes to the connected serial port and return wire bytes.

        Diagnostic services use this method for binary UDS requests such as
        ``22 F1 80``.  The payload is wrapped by the BEA frame markers unless
        ``framed=False`` is explicitly requested.
        """
        try:
            data = bytes(payload)
        except (TypeError, ValueError) as exc:
            raise InvalidPayloadError("Binary payload must be bytes-like.") from exc
        if not data:
            raise EmptyCommandError("The diagnostic request cannot be empty.")

        wire_payload = self._prepare_payload(data, framed=framed)
        self._send_payload(wire_payload)
        self._logger.info("TX: [Hex] %s", data.hex(" ").upper())
        return wire_payload

    @staticmethod
    def _prepare_payload(payload: bytes, *, framed: bool) -> bytes:
        return frame_payload(payload) if framed else payload

    def _send_payload(self, payload: bytes) -> None:
        """Write and flush a payload while applying common error handling."""
        with self._connection_lock:
            connection = self._connection
            if connection is None or not connection.is_open:
                raise NotConnectedError("Not connected. The command was not sent.")

            try:
                connection.write(payload)
                connection.flush()
            except (serial.SerialException, OSError) as exc:
                self._mark_connection_lost(connection)
                raise SerialWriteError(f"Could not write to the serial port: {exc}") from exc

    def _read_loop(self, connection: SerialConnection) -> None:
        """Read incoming lines until the connection is stopped or lost."""
        while not self._stop_reader.is_set():
            try:
                data = connection.readline()
            except (serial.SerialException, OSError) as exc:
                if not self._stop_reader.is_set():
                    self._mark_connection_lost(connection)
                    self._logger.error("Serial read failed: %s", exc)
                return

            if not data:
                continue

            data = remove_frame_markers(data)
            if not data:
                continue

            if all(byte in (9, 10, 13) or 32 <= byte <= 126 for byte in data):
                text = data.decode("utf-8", errors="replace")
                lines = text.splitlines() or [text]
                for line in lines:
                    self._logger.info("RX: [String] %s", line)
            else:
                self._logger.info("RX: [Hex] %s", data.hex(" ").upper())

    def _mark_connection_lost(self, connection: SerialConnection) -> None:
        """Invalidate a connection and notify the UI without touching Tk."""
        with self._connection_lock:
            if self._connection is not connection:
                return
            self._connection = None
            self._stop_reader.set()

        try:
            if connection.is_open:
                connection.close()
        except (serial.SerialException, OSError) as exc:
            self._logger.warning("Error while closing the lost serial port: %s", exc)

        if self._on_connection_lost is not None:
            self._on_connection_lost()


__all__ = ["SerialManager"]
