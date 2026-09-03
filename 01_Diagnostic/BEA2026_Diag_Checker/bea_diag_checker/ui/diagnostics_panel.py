"""Reusable diagnostic-action panel for the main window."""

from __future__ import annotations

from collections.abc import Callable, Sequence
import tkinter as tk
from tkinter import ttk


class DiagnosticsPanel:
    """Build the Security Access, Read DID, and Write DID controls.

    The panel owns only UI state.  Protocol behavior is supplied through the
    callback arguments, which keeps the controls reusable and leaves the
    application-specific UDS implementation in the main window.
    """

    def __init__(
        self,
        parent: ttk.Frame,
        *,
        did_options: Sequence[str],
        on_request_seed: Callable[[], None],
        on_send_key: Callable[[], None],
        on_read_did: Callable[[], None],
        on_write_did: Callable[[], None],
    ) -> None:
        self.frame = ttk.LabelFrame(
            parent,
            text="  Diagnostic actions  ",
            style="Card.TLabelframe",
            padding=10,
        )
        self.frame.columnconfigure(0, weight=1, uniform="diagnostic-card")
        self.frame.columnconfigure(1, weight=1, uniform="diagnostic-card")
        self.frame.columnconfigure(2, weight=1, uniform="diagnostic-card")
        self._action_buttons: list[ttk.Button] = []

        did_values = tuple(did_options)
        default_did = did_values[0] if did_values else ""

        self._create_security_card(
            on_request_seed=on_request_seed,
            on_send_key=on_send_key,
        )
        self._create_read_card(
            did_values=did_values,
            default_did=default_did,
            on_read_did=on_read_did,
        )
        self._create_write_card(
            did_values=did_values,
            default_did=default_did,
            on_write_did=on_write_did,
        )

    @property
    def read_did(self) -> str:
        """Return the DID currently selected in the Read DID control."""
        return self.read_did_var.get()

    @property
    def write_did(self) -> str:
        """Return the DID currently selected in the Write DID control."""
        return self.write_did_var.get()

    @property
    def write_value(self) -> str:
        """Return the string currently entered for Write DID."""
        return self.write_value_var.get()

    def set_connected(self, connected: bool) -> None:
        """Enable protocol action buttons only while a COM port is connected."""
        state = "normal" if connected else "disabled"
        for button in self._action_buttons:
            button.configure(state=state)

    def _create_security_card(
        self,
        *,
        on_request_seed: Callable[[], None],
        on_send_key: Callable[[], None],
    ) -> None:
        card = ttk.LabelFrame(
            self.frame,
            text="  Security Access  ",
            style="Card.TLabelframe",
            padding=10,
        )
        card.grid(row=0, column=0, sticky="nsew", padx=(0, 6))
        card.columnconfigure(0, weight=1)
        card.columnconfigure(1, weight=1)
        ttk.Label(
            card,
            text="Use the reserved callbacks for seed/key handling.",
            style="Muted.TLabel",
            wraplength=250,
            justify=tk.LEFT,
        ).grid(row=0, column=0, columnspan=2, sticky="w", pady=(0, 10))

        request_seed_button = ttk.Button(
            card,
            text="Request Seed",
            style="Secondary.TButton",
            command=on_request_seed,
        )
        request_seed_button.grid(row=1, column=0, sticky="ew", padx=(0, 4))
        send_key_button = ttk.Button(
            card,
            text="Send Key",
            style="Secondary.TButton",
            command=on_send_key,
        )
        send_key_button.grid(row=1, column=1, sticky="ew", padx=(4, 0))
        self._action_buttons = [request_seed_button, send_key_button]

    def _create_read_card(
        self,
        *,
        did_values: tuple[str, ...],
        default_did: str,
        on_read_did: Callable[[], None],
    ) -> None:
        card = ttk.LabelFrame(
            self.frame,
            text="  Read DID  ",
            style="Card.TLabelframe",
            padding=10,
        )
        card.grid(row=0, column=1, sticky="nsew", padx=6)
        card.columnconfigure(1, weight=1)

        ttk.Label(card, text="DID", style="Card.TLabel").grid(
            row=0, column=0, sticky="w", padx=(0, 8)
        )
        self.read_did_var = tk.StringVar(value=default_did)
        self.read_did_combo = ttk.Combobox(
            card,
            textvariable=self.read_did_var,
            values=did_values,
            state="readonly",
            width=16,
        )
        self.read_did_combo.grid(row=0, column=1, sticky="ew")

        read_button = ttk.Button(
            card,
            text="Read",
            style="Primary.TButton",
            command=on_read_did,
        )
        read_button.grid(row=1, column=0, columnspan=2, sticky="ew", pady=(12, 0))
        self._action_buttons.append(read_button)

    def _create_write_card(
        self,
        *,
        did_values: tuple[str, ...],
        default_did: str,
        on_write_did: Callable[[], None],
    ) -> None:
        card = ttk.LabelFrame(
            self.frame,
            text="  Write DID  ",
            style="Card.TLabelframe",
            padding=10,
        )
        card.grid(row=0, column=2, sticky="nsew", padx=(6, 0))
        card.columnconfigure(1, weight=1)

        ttk.Label(card, text="DID", style="Card.TLabel").grid(
            row=0, column=0, sticky="w", padx=(0, 8)
        )
        self.write_did_var = tk.StringVar(value=default_did)
        self.write_did_combo = ttk.Combobox(
            card,
            textvariable=self.write_did_var,
            values=did_values,
            state="readonly",
            width=16,
        )
        self.write_did_combo.grid(row=0, column=1, sticky="ew")

        ttk.Label(card, text="Value", style="Card.TLabel").grid(
            row=1, column=0, sticky="w", padx=(0, 8), pady=(10, 0)
        )
        self.write_value_var = tk.StringVar()
        self.write_value_entry = ttk.Entry(
            card,
            textvariable=self.write_value_var,
        )
        self.write_value_entry.grid(row=1, column=1, sticky="ew", pady=(10, 0))

        write_button = ttk.Button(
            card,
            text="Write",
            style="Primary.TButton",
            command=on_write_did,
        )
        write_button.grid(row=2, column=0, columnspan=2, sticky="ew", pady=(12, 0))
        self._action_buttons.append(write_button)


__all__ = ["DiagnosticsPanel"]
