from bea_diag_checker.core.Diagnostic.codec import parse_hex_bytes


def test_parse_hex_bytes_preserves_leading_zero_bytes() -> None:
    assert parse_hex_bytes("0x00001122", field_name="Data") == b"\x00\x00\x11\x22"