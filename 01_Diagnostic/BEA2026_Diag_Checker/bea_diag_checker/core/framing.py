"""Wire-frame helpers for the BEA serial protocol."""

from __future__ import annotations

from ..config.constants import END_OF_FRAME, START_OF_FRAME


def frame_payload(payload: bytes) -> bytes:
    """Wrap payload bytes with the configured BEA start and end markers."""
    return START_OF_FRAME + payload + END_OF_FRAME


def remove_frame_markers(data: bytes) -> bytes:
    """Remove BEA frame markers from data shown to the user.

    The transport still writes and receives the complete framed bytes.  This
    helper is deliberately used only for presentation, so the activity log
    shows the command or response payload rather than protocol envelope bytes.
    Removing occurrences rather than only the first and last marker also
    handles multiple frames returned in one serial read.
    """
    return data.replace(START_OF_FRAME, b"").replace(END_OF_FRAME, b"")


__all__ = ["frame_payload", "remove_frame_markers"]
