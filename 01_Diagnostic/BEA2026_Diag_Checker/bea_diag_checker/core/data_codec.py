"""Input codecs used by the generic serial command editor."""

from __future__ import annotations

import re

from .exceptions import InvalidPayloadError

_HEX_SEPARATOR_PATTERN = re.compile(r"[\s,:\-_]+")


def parse_hex_payload(value: str) -> bytes:
    """Parse a user-entered hexadecimal payload.

    The input may use spaces, commas, colons, hyphens, underscores, and
    optional ``0x`` prefixes, for example ``0x22 0xF1 0x80``.  Frame markers
    are added by the serial transport after parsing.
    """
    if not isinstance(value, str):
        raise InvalidPayloadError("Hex data must be entered as a string.")

    normalized = _HEX_SEPARATOR_PATTERN.sub("", value.strip())
    normalized = normalized.replace("0x", "").replace("0X", "")
    if not normalized:
        raise InvalidPayloadError("Enter hexadecimal data before sending.")
    if len(normalized) % 2 != 0:
        raise InvalidPayloadError(
            "Hex data must contain complete byte values (two hex digits per byte)."
        )

    try:
        return bytes.fromhex(normalized)
    except ValueError as exc:
        raise InvalidPayloadError(
            "Hex data may contain only hexadecimal digits and separators."
        ) from exc


__all__ = ["parse_hex_payload"]
