"""Small helpers for validating hexadecimal diagnostic input."""

from __future__ import annotations

from collections.abc import Buffer

from ..exceptions import InvalidDiagnosticRequestError


def parse_hex_bytes(
    value: object,
    *,
    field_name: str,
    expected_length: int | None = None,
    allow_empty: bool = False,
) -> bytes:
    """Convert common hexadecimal input forms to bytes.

    Strings may contain an optional leading ``0x`` prefix and separators such
    as spaces, colons, hyphens, or underscores. Leading zeroes are retained as
    zero bytes. Integer input is accepted for small fields such as a one-byte
    security-access sub-function.
    """
    if isinstance(value, bytes):
        result = value
    elif isinstance(value, bytearray):
        result = bytes(value)
    elif isinstance(value, memoryview):
        result = value.tobytes()
    elif isinstance(value, int) and not isinstance(value, bool):
        if value < 0:
            raise InvalidDiagnosticRequestError(
                f"{field_name} must not be negative."
            )
        if expected_length is not None:
            maximum = (1 << (expected_length * 8)) - 1
            if value > maximum:
                raise InvalidDiagnosticRequestError(
                    f"{field_name} must fit in {expected_length} byte(s)."
                )
            result = value.to_bytes(expected_length, byteorder="big")
        else:
            length = max(1, (value.bit_length() + 7) // 8)
            result = value.to_bytes(length, byteorder="big")
    elif isinstance(value, str):
        normalized = value.strip()
        if normalized[:2].lower() == "0x":
            normalized = normalized[2:]
        for separator in (" ", "\t", "\r", "\n", ":", "-", "_"):
            normalized = normalized.replace(separator, "")
        if not normalized:
            result = b""
        elif len(normalized) % 2 != 0:
            raise InvalidDiagnosticRequestError(
                f"{field_name} must contain complete hexadecimal bytes."
            )
        else:
            try:
                result = bytes.fromhex(normalized)
            except ValueError as exc:
                raise InvalidDiagnosticRequestError(
                    f"{field_name} must be hexadecimal."
                ) from exc
    elif isinstance(value, Buffer):
        result = bytes(value)
    else:
        raise InvalidDiagnosticRequestError(
            f"{field_name} must be bytes, an integer, or a hexadecimal string."
        )

    if not result and not allow_empty:
        raise InvalidDiagnosticRequestError(f"{field_name} must not be empty.")
    if expected_length is not None and len(result) != expected_length:
        raise InvalidDiagnosticRequestError(
            f"{field_name} must contain exactly {expected_length} byte(s); "
            f"received {len(result)}."
        )
    return result


__all__ = ["parse_hex_bytes"]
