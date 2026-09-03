"""Backward-compatible launcher for BEA Diag Checker."""

from bea_diag_checker.main import main
from bea_diag_checker.ui.main_window import BEADiagChecker

__all__ = ["BEADiagChecker", "main"]


if __name__ == "__main__":
    main()