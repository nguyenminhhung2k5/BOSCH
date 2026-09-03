"""Application entry point."""

from __future__ import annotations

import tkinter as tk

from .ui.main_window import BEADiagChecker


def main() -> None:
    """Start the BEA Diag Checker window."""
    try:
        root = tk.Tk()
    except tk.TclError as exc:
        raise SystemExit(
            "Unable to start the GUI. Install Python 3.12.13.1 with Tcl/Tk support."
        ) from exc
    BEADiagChecker(root)
    root.mainloop()


__all__ = ["main"]


if __name__ == "__main__":
    main()
