"""UDS Security Access service (0x27).

The generic ``process`` function supports both Request Seed and Send Key
sub-functions.  The application can implement its seed/key algorithm later
without changing the diagnostic dispatcher.
"""

from __future__ import annotations

from collections.abc import Mapping
from dataclasses import dataclass
import logging

from ..exceptions import InvalidDiagnosticRequestError
from ..serial_manager import SerialManager
from .codec import parse_hex_bytes

SERVICE_ID = 0x27
SERVICE_NAME = "security_access"


@dataclass(frozen=True)
class SecurityAccessRequest:
    """A security-access sub-function and its optional key/seed data."""

    sub_function: int
    data: bytes = b""


def _parse_sub_function(value: object) -> int:
    raw = parse_hex_bytes(value, field_name="security sub-function", expected_length=1)
    sub_function = raw[0]
    if sub_function == 0:
        raise InvalidDiagnosticRequestError(
            "Security sub-function must be between 0x01 and 0xFF."
        )
    return sub_function


def _parse_request(request_data: object) -> SecurityAccessRequest:
    if isinstance(request_data, SecurityAccessRequest):
        sub_function = _parse_sub_function(request_data.sub_function)
        data = parse_hex_bytes(
            request_data.data,
            field_name="security data",
            allow_empty=True,
        )
    elif isinstance(request_data, Mapping):
        if "sub_function" in request_data:
            sub_function_value = request_data["sub_function"]
        elif "level" in request_data:
            sub_function_value = request_data["level"]
        else:
            raise InvalidDiagnosticRequestError(
                "Security request must contain 'sub_function' or 'level'."
            )
        sub_function = _parse_sub_function(sub_function_value)
        data_value = request_data.get("data", request_data.get("key", b""))
        data = parse_hex_bytes(
            data_value,
            field_name="security data",
            allow_empty=True,
        )
    elif isinstance(request_data, (bytes, bytearray, memoryview)):
        raw = parse_hex_bytes(
            request_data,
            field_name="security request",
        )
        sub_function = raw[0]
        if sub_function == 0:
            raise InvalidDiagnosticRequestError(
                "Security sub-function must be between 0x01 and 0xFF."
            )
        data = raw[1:]
    else:
        raw = parse_hex_bytes(request_data, field_name="security request")
        sub_function = raw[0]
        if sub_function == 0:
            raise InvalidDiagnosticRequestError(
                "Security sub-function must be between 0x01 and 0xFF."
            )
        data = raw[1:]

    # Standard UDS security levels use odd sub-functions for Request Seed and
    # the following even sub-function for Send Key.  A key is required for a
    # Send Key request, while a Request Seed request may have no data.
    if sub_function & 0x7F and not (sub_function & 0x01) and not data:
        raise InvalidDiagnosticRequestError(
            "Send Key requests require key data."
        )
    return SecurityAccessRequest(sub_function, data)


def process(
    serial_manager: SerialManager,
    request_data: object,
    logger: logging.Logger,
    data_format: str = "Hex",
) -> bytes:
    """Build and send a UDS Security Access request.

    Accepted input forms:

    * ``SecurityAccessRequest(0x01)`` for Request Seed.
    * ``SecurityAccessRequest(0x02, b"...")`` for Send Key.
    * A mapping with ``sub_function``/``level`` and optional ``data``/``key``.
    * Hex bytes such as ``b"\x01"`` or ``"01 AA BB"``.

    The returned value is the exact framed bytes written to the serial port.
    ``data_format`` selects binary Hex or ASCII String transmission. Seed/key
    response handling remains outside this transport layer.
    """
    parsed = _parse_request(request_data)
    request = bytes((SERVICE_ID, parsed.sub_function)) + parsed.data
    wire_payload = serial_manager.send_service_payload(request, data_format)
    logger.info(
        "UDS 0x27 Security Access request: sub-function=0x%02X, data length=%d",
        parsed.sub_function,
        len(parsed.data),
    )
    return wire_payload


def request_seed(
    serial_manager: SerialManager,
    level: int = 0x01,
    logger: logging.Logger | None = None,
) -> bytes:
    """Convenience wrapper for a Request Seed sub-function."""
    if logger is None:
        logger = logging.getLogger(__name__)
    if level & 0x01 == 0:
        raise InvalidDiagnosticRequestError("Request Seed level must be odd.")
    return process(serial_manager, SecurityAccessRequest(level), logger)


def send_key(
    serial_manager: SerialManager,
    level: int,
    key: object,
    logger: logging.Logger | None = None,
) -> bytes:
    """Convenience wrapper for the Send Key sub-function after ``level``."""
    if logger is None:
        logger = logging.getLogger(__name__)
    if level & 0x01 == 0:
        raise InvalidDiagnosticRequestError("Security level must be odd.")
    key_bytes = parse_hex_bytes(key, field_name="key")
    return process(
        serial_manager,
        SecurityAccessRequest(level + 1, key_bytes),
        logger,
    )


__all__ = [
    "SERVICE_ID",
    "SERVICE_NAME",
    "SecurityAccessRequest",
    "process",
    "request_seed",
    "send_key",
]
