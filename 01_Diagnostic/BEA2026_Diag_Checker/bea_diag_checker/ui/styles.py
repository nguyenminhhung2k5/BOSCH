"""Visual theme for the Tkinter application."""

from __future__ import annotations

import tkinter as tk
from tkinter import ttk


COLORS = {
    "app": "#eef2f7",
    "card": "#ffffff",
    "border": "#d9e1ec",
    "text": "#172033",
    "muted": "#64748b",
    "accent": "#2563eb",
    "accent_hover": "#1d4ed8",
    "success": "#15803d",
    "danger": "#b42318",
    "terminal": "#111827",
    "terminal_text": "#dbeafe",
}


def configure_styles(root: tk.Misc) -> dict[str, str]:
    """Configure ttk styles and return a copy of the color palette."""
    colors = COLORS.copy()
    style = ttk.Style(root)
    try:
        style.theme_use("clam")
    except tk.TclError:
        pass

    style.configure("App.TFrame", background=colors["app"])
    style.configure("Card.TFrame", background=colors["card"])
    style.configure(
        "Card.TLabelframe",
        background=colors["card"],
        bordercolor=colors["border"],
        lightcolor=colors["border"],
        darkcolor=colors["border"],
        relief="solid",
        borderwidth=1,
    )
    style.configure(
        "Card.TLabelframe.Label",
        background=colors["card"],
        foreground=colors["text"],
        font=("Segoe UI Semibold", 10),
    )
    style.configure(
        "Title.TLabel",
        background=colors["app"],
        foreground=colors["text"],
        font=("Segoe UI Semibold", 22),
    )
    style.configure(
        "Subtitle.TLabel",
        background=colors["app"],
        foreground=colors["muted"],
        font=("Segoe UI", 10),
    )
    style.configure(
        "Card.TLabel",
        background=colors["card"],
        foreground=colors["text"],
    )
    style.configure(
        "Muted.TLabel",
        background=colors["card"],
        foreground=colors["muted"],
        font=("Segoe UI", 9),
    )
    style.configure(
        "Footer.TLabel",
        background=colors["app"],
        foreground=colors["muted"],
        font=("Segoe UI", 9),
    )
    style.configure(
        "Card.TCheckbutton",
        background=colors["card"],
        foreground=colors["muted"],
    )
    style.configure(
        "Primary.TButton",
        background=colors["accent"],
        foreground="#ffffff",
        borderwidth=0,
        padding=(16, 8),
        font=("Segoe UI Semibold", 10),
    )
    style.map(
        "Primary.TButton",
        background=[
            ("disabled", "#a8b7d1"),
            ("pressed", colors["accent_hover"]),
            ("active", colors["accent_hover"]),
        ],
        foreground=[("disabled", "#e7edf7"), ("!disabled", "#ffffff")],
    )
    style.configure(
        "Secondary.TButton",
        background="#f8fafc",
        foreground=colors["text"],
        bordercolor=colors["border"],
        padding=(12, 7),
    )
    style.map(
        "Secondary.TButton",
        background=[("active", "#e8eef7"), ("pressed", "#dce6f3")],
    )
    style.configure(
        "Connected.Status.TLabel",
        background=colors["app"],
        foreground=colors["success"],
        font=("Segoe UI Semibold", 10),
    )
    style.configure(
        "Disconnected.Status.TLabel",
        background=colors["app"],
        foreground=colors["muted"],
        font=("Segoe UI Semibold", 10),
    )
    style.configure(
        "Error.Status.TLabel",
        background=colors["app"],
        foreground=colors["danger"],
        font=("Segoe UI Semibold", 10),
    )
    return colors


__all__ = ["COLORS", "configure_styles"]
