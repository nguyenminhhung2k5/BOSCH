"""UDS Read Data By Identifier service (0x22).

The service receives a two-byte DID such as ``0xF180`` and sends the UDS
request ``22 F1 80`` through :class:`SerialManager`.  The serial manager wraps
that request with the configured BEA start and end frame markers. The serial
reader keeps receiving asynchronously, so the device response is reported by
the normal RX logging path rather than being returned synchronously from this
function.
"""

from __future__ import annotations

import logging

from bea_diag_checker.core.diagnostic_manager import DiagnosticRequestFromUI

from ..exceptions import InvalidDiagnosticRequestError
from ..serial_manager import SerialManager
from .codec import parse_hex_bytes

SERVICE_ID = 0x22
SERVICE_NAME = "read_DID"


def _parse_did(did: object) -> bytes:
    """Validate and normalize a DID to exactly two bytes."""
    try:
        return parse_hex_bytes(did, field_name="DID", expected_length=2)
    except InvalidDiagnosticRequestError:
        raise
    except (TypeError, ValueError) as exc:
        raise InvalidDiagnosticRequestError("DID must be a two-byte hexadecimal value.") from exc


def process(
    serial_manager: SerialManager,
    request_data: DiagnosticRequestFromUI,
    logger: logging.Logger,
    data_format: str = "Hex",
) -> bytes:
    """Build and send a Read DID request.

    ``request_data`` accepts ``0xF180``, ``F180``, a two-byte ``bytes`` value,
    or the integer ``0xF180``.  The returned value is the exact framed bytes
    written to the serial port.  The device response is read by the serial
    manager's background reader. ``data_format`` controls whether the
    generated request is transmitted as binary Hex bytes or ASCII String data.
    """
    did = _parse_did(request_data.DID)
    request = bytes((SERVICE_ID,)) + did
    wire_payload = serial_manager.send_service_payload(request, data_format)
    logger.info("UDS 0x22 Read DID request: DID=0x%04X", int.from_bytes(did, "big"))
    return wire_payload


__all__ = ["SERVICE_ID", "SERVICE_NAME", "process"]
