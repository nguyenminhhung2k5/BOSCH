"""Centralized application configuration."""

from __future__ import annotations

from pathlib import Path

APP_NAME = "BEA Diag Checker"
APP_VERSION = "1.0.0"
REQUIRED_PYTHON = "3.12.13.1"

BAUD_RATES = (9600, 19200, 38400, 57600, 115200)
DEFAULT_BAUD_RATE = 115200

DATA_FORMATS = ("String", "Hex")
DEFAULT_DATA_FORMAT = "Hex"

# Every payload sent through the generic terminal and diagnostic services is
# wrapped with these protocol markers.  Keep the values as bytes so they are
# never accidentally encoded as text.
START_OF_FRAME = bytes((0x0F, 0xFF, 0xF0))
END_OF_FRAME = bytes((0xF0, 0x00, 0x0F))

# Replace or extend this device-specific list as the BEA diagnostic database
# becomes available.  Keeping it in configuration makes both DID selectors
# consistent and avoids hardcoding protocol data in the UI layout.
DID_OPTIONS = (
    "0x0123",
    "0x0124",
)

LINE_ENDINGS = {
    "None": "",
    "LF (\\n)": "\n",
    "CRLF (\\r\\n)": "\r\n",
}
DEFAULT_LINE_ENDING = "CRLF (\\r\\n)"

COMMAND_HISTORY_LIMIT = 50
SERIAL_READ_TIMEOUT_SECONDS = 0.1
SERIAL_WRITE_TIMEOUT_SECONDS = 1.0

# Keep the runtime log beside the launcher script so it is easy to find on Windows.
PROJECT_ROOT = Path(__file__).resolve().parents[2]
LOG_FILE_PATH = PROJECT_ROOT / "bea_diag_checker.log"
