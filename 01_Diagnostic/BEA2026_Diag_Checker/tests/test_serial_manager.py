from __future__ import annotations

import logging
import threading

import pytest
import serial

from bea_diag_checker.core import (
    EmptyCommandError,
    InvalidPayloadError,
    NotConnectedError,
    SerialConnectionError,
    SerialManager,
    SerialWriteError,
)
from bea_diag_checker.config import END_OF_FRAME, START_OF_FRAME
from bea_diag_checker.core.framing import remove_frame_markers


class FakeSerial:
    def __init__(
        self,
        *,
        fail_on_write: bool = False,
        incoming: bytes | None = None,
        **_kwargs: object,
    ) -> None:
        self.is_open = True
        self.fail_on_write = fail_on_write
        self.incoming = incoming
        self.incoming_sent = False
        self.writes: list[bytes] = []
        self.closed = False

    def close(self) -> None:
        self.is_open = False
        self.closed = True

    def flush(self) -> None:
        if not self.is_open:
            raise serial.SerialException("closed")

    def readline(self) -> bytes:
        if self.incoming is not None and not self.incoming_sent:
            self.incoming_sent = True
            return self.incoming
        return b""

    def write(self, data: bytes) -> int:
        if self.fail_on_write:
            raise serial.SerialException("write failed")
        if not self.is_open:
            raise serial.SerialException("closed")
        self.writes.append(data)
        return len(data)


class FakeFactory:
    def __init__(self, **serial_kwargs: object) -> None:
        self.serial_kwargs = serial_kwargs
        self.connection = FakeSerial()

    def __call__(self, **kwargs: object) -> FakeSerial:
        self.serial_kwargs = kwargs
        return self.connection


def make_manager(factory: FakeFactory | None = None) -> tuple[SerialManager, FakeFactory]:
    serial_factory = factory or FakeFactory()
    logger = logging.getLogger("test_serial_manager")
    logger.handlers.clear()
    logger.addHandler(logging.NullHandler())
    return SerialManager(logger, serial_factory=serial_factory), serial_factory


def test_connect_uses_selected_port_and_baud_rate() -> None:
    manager, factory = make_manager()

    manager.connect("COM7", 115200)

    assert factory.serial_kwargs == {
        "port": "COM7",
        "baudrate": 115200,
        "timeout": 0.1,
        "write_timeout": 1.0,
    }
    assert manager.is_connected
    manager.disconnect(announce=False)


def test_send_text_appends_line_ending_and_encodes_utf8() -> None:
    manager, factory = make_manager()
    manager.connect("COM7", 9600)

    payload = manager.send_text("測試", "\r\n")

    assert payload == START_OF_FRAME + "測試\r\n".encode("utf-8") + END_OF_FRAME
    assert factory.connection.writes == [payload]
    manager.disconnect(announce=False)


def test_send_bytes_preserves_binary_payload() -> None:
    manager, factory = make_manager()
    manager.connect("COM7", 115200)

    payload = manager.send_bytes(b"\x22\xF1\x80")

    assert payload == START_OF_FRAME + b"\x22\xF1\x80" + END_OF_FRAME
    assert factory.connection.writes == [payload]
    manager.disconnect(announce=False)


def test_send_hex_accepts_prefixed_and_separated_values() -> None:
    manager, factory = make_manager()
    manager.connect("COM7", 115200)

    payload = manager.send_hex("0x22 0xF1, 0x80")

    assert payload == START_OF_FRAME + b"\x22\xF1\x80" + END_OF_FRAME
    assert factory.connection.writes == [payload]
    manager.disconnect(announce=False)


def test_send_hex_rejects_invalid_input() -> None:
    manager, _ = make_manager()

    with pytest.raises(InvalidPayloadError, match="hexadecimal"):
        manager.send_hex("0x2G")


def test_send_service_payload_as_string_uses_ascii_hex_text() -> None:
    manager, factory = make_manager()
    manager.connect("COM7", 115200)

    payload = manager.send_service_payload(b"\x22\xF1\x80", "String")

    assert payload == START_OF_FRAME + b"22F180" + END_OF_FRAME
    assert factory.connection.writes == [payload]
    manager.disconnect(announce=False)


def test_send_service_payload_rejects_unknown_format() -> None:
    manager, _ = make_manager()

    with pytest.raises(InvalidPayloadError, match="Data format"):
        manager.send_service_payload(b"\x22", "Base64")


def test_frame_markers_are_removed_from_log_payloads() -> None:
    assert remove_frame_markers(
        START_OF_FRAME + b"ABC" + END_OF_FRAME
    ) == b"ABC"


def test_tx_log_does_not_show_frame_markers() -> None:
    logger = logging.getLogger("test_tx_log_without_frame")
    logger.handlers.clear()
    messages: list[str] = []

    class CaptureHandler(logging.Handler):
        def emit(self, record: logging.LogRecord) -> None:
            messages.append(record.getMessage())

    logger.setLevel(logging.INFO)
    logger.addHandler(CaptureHandler())
    factory = FakeFactory()
    manager = SerialManager(logger, serial_factory=factory)
    manager.connect("COM7", 115200)
    manager.send_hex("41 42 43")
    manager.disconnect(announce=False)

    tx_messages = [message for message in messages if message.startswith("TX:")]
    assert tx_messages == ["TX: [Hex] 41 42 43"]
    assert "0F FF F0" not in tx_messages[0]
    assert "F0 00 0F" not in tx_messages[0]


def test_rx_log_does_not_show_frame_markers() -> None:
    logger = logging.getLogger("test_rx_log_without_frame")
    logger.handlers.clear()
    messages: list[str] = []
    rx_logged = threading.Event()

    class CaptureHandler(logging.Handler):
        def emit(self, record: logging.LogRecord) -> None:
            message = record.getMessage()
            messages.append(message)
            if message.startswith("RX:"):
                rx_logged.set()

    logger.setLevel(logging.INFO)
    logger.addHandler(CaptureHandler())
    factory = FakeFactory()
    factory.connection = FakeSerial(
        incoming=START_OF_FRAME + b"ABC" + END_OF_FRAME
    )
    manager = SerialManager(logger, serial_factory=factory)
    manager.connect("COM7", 115200)
    assert rx_logged.wait(timeout=1.0)
    manager.disconnect(announce=False)

    rx_messages = [message for message in messages if message.startswith("RX:")]
    assert rx_messages == ["RX: [String] ABC"]
    assert "0F FF F0" not in rx_messages[0]
    assert "F0 00 0F" not in rx_messages[0]


def test_send_text_rejects_empty_command() -> None:
    manager, _ = make_manager()

    with pytest.raises(EmptyCommandError):
        manager.send_text("  ")


def test_send_text_requires_connection() -> None:
    manager, _ = make_manager()

    with pytest.raises(NotConnectedError):
        manager.send_text("PING")


def test_connect_wraps_serial_errors() -> None:
    def failing_factory(**_kwargs: object) -> FakeSerial:
        raise serial.SerialException("access denied")

    manager = SerialManager(logging.getLogger("test_connect_error"), serial_factory=failing_factory)

    with pytest.raises(SerialConnectionError, match="access denied"):
        manager.connect("COM1", 115200)


def test_send_text_wraps_write_errors() -> None:
    factory = FakeFactory()
    factory.connection = FakeSerial(fail_on_write=True)
    manager, _ = make_manager(factory)
    manager.connect("COM7", 115200)

    with pytest.raises(SerialWriteError):
        manager.send_text("PING")

    assert not manager.is_connected
