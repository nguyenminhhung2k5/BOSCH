"""Logging helpers shared by the desktop application."""

from __future__ import annotations

import logging
import queue
from pathlib import Path


class QueueLogHandler(logging.Handler):
    """Send formatted log messages to a queue consumed by the Tk UI."""

    def __init__(self, log_queue: queue.Queue[str]) -> None:
        super().__init__()
        self.log_queue = log_queue

    def emit(self, record: logging.LogRecord) -> None:
        try:
            self.log_queue.put(self.format(record))
        except Exception:
            self.handleError(record)


def configure_logging(
    logger_name: str,
    log_path: Path,
    log_queue: queue.Queue[str],
) -> tuple[logging.Logger, logging.Handler, QueueLogHandler]:
    """Configure file and UI handlers and return them for later cleanup."""
    logger = logging.getLogger(logger_name)
    logger.setLevel(logging.DEBUG)
    logger.propagate = False

    # Avoid duplicate output if a window is recreated in one Python process.
    for handler in list(logger.handlers):
        logger.removeHandler(handler)
        handler.close()

    file_handler = logging.FileHandler(log_path, encoding="utf-8")
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(
        logging.Formatter(
            "%(asctime)s - %(levelname)s - %(message)s",
            datefmt="%Y-%m-%d %H:%M:%S",
        )
    )

    ui_handler = QueueLogHandler(log_queue)
    ui_handler.setLevel(logging.DEBUG)
    ui_handler.setFormatter(
        logging.Formatter(
            "[%(asctime)s] %(levelname)s: %(message)s",
            datefmt="%H:%M:%S",
        )
    )

    logger.addHandler(file_handler)
    logger.addHandler(ui_handler)
    return logger, file_handler, ui_handler


__all__ = ["QueueLogHandler", "configure_logging"]
