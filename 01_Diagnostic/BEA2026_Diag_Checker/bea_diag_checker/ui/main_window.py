"""Main Tkinter window for BEA Diag Checker."""

from __future__ import annotations

from dataclasses import dataclass
import queue
import sys
import tkinter as tk
from tkinter import scrolledtext, ttk

import serial
from serial.tools import list_ports

from ..config import (
    APP_VERSION as CONFIG_APP_VERSION,
    BAUD_RATES as CONFIG_BAUD_RATES,
    COMMAND_HISTORY_LIMIT,
    DATA_FORMATS,
    DEFAULT_BAUD_RATE,
    DEFAULT_DATA_FORMAT,
    DID_OPTIONS as CONFIG_DID_OPTIONS,
    DEFAULT_LINE_ENDING,
    END_OF_FRAME,
    LINE_ENDINGS as CONFIG_LINE_ENDINGS,
    LOG_FILE_PATH,
    REQUIRED_PYTHON,
    START_OF_FRAME,
)
from ..core import (
    DiagnosticManager,
    DiagnosticManagerError,
    DiagnosticRequestFromUI,
    EmptyCommandError,
    InvalidDiagnosticRequestError,
    InvalidPayloadError,
    NotConnectedError,
    SerialConnectionError,
    SerialManager,
    SerialManagerError,
    SerialWriteError,
)
from ..core.Diagnostic.codec import parse_hex_bytes
from ..utils import configure_logging
from .diagnostics_panel import DiagnosticsPanel
from .styles import configure_styles


@dataclass(frozen=True)
class CommandHistoryItem:
    """One previously sent command and the options used to send it."""

    input_data: str
    data_format: str
    line_ending: str


class BEADiagChecker:
    """Coordinate the Tkinter interface and the serial communication service."""

    APP_VERSION = CONFIG_APP_VERSION
    REQUIRED_PYTHON = REQUIRED_PYTHON
    BAUD_RATES = CONFIG_BAUD_RATES
    DID_OPTIONS = CONFIG_DID_OPTIONS
    LINE_ENDINGS = CONFIG_LINE_ENDINGS

    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("BEA Diag Checker")
        self.root.geometry("1080x780")
        self.root.minsize(820, 640)
        self.root.configure(background="#eef2f7")
        self.root.option_add("*Font", ("Segoe UI", 10))
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

        self.log_queue: queue.Queue[str] = queue.Queue()
        self.command_history: list[CommandHistoryItem] = []
        self.history_index: int | None = None
        self.log_message_count = 0
        self._closing = False

        self.logger, self.file_handler, self.ui_handler = configure_logging(
            "bea_diag_checker",
            LOG_FILE_PATH,
            self.log_queue,
        )
        self.serial_manager = SerialManager(
            self.logger,
            on_connection_lost=self._schedule_connection_lost,
        )
        self.diagnostic_manager = DiagnosticManager(self.serial_manager, self.logger)
        self._last_calculated_key: bytes | None = None

        self.colors = configure_styles(self.root)
        self._create_widgets()
        self._poll_log_queue()
        self._refresh_ports()
        self.logger.info(
            "Ready - Python %s, pyserial %s",
            ".".join(str(part) for part in sys.version_info[:3]),
            getattr(serial, "__version__", "unknown"),
        )

    def _create_widgets(self) -> None:
        """Build the responsive application interface."""
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)

        app_frame = ttk.Frame(self.root, style="App.TFrame", padding=(18, 14, 18, 10))
        app_frame.grid(row=0, column=0, sticky="nsew")
        app_frame.columnconfigure(0, weight=1)
        app_frame.rowconfigure(3, weight=1)

        header = ttk.Frame(app_frame, style="App.TFrame")
        header.grid(row=0, column=0, sticky="ew", pady=(0, 14))
        header.columnconfigure(0, weight=1)
        ttk.Label(header, text="BEA Diag Checker", style="Title.TLabel").grid(
            row=0, column=0, sticky="w"
        )
        ttk.Label(
            header,
            text="Serial diagnostics workspace",
            style="Subtitle.TLabel",
        ).grid(row=1, column=0, sticky="w", pady=(2, 0))
        self.status_label = ttk.Label(
            header,
            text="●  Disconnected",
            style="Disconnected.Status.TLabel",
        )
        self.status_label.grid(row=0, column=1, rowspan=2, sticky="e", padx=(18, 0))

        self._create_connection_section(app_frame)
        self._create_diagnostics_section(app_frame)
        self._create_log_section(app_frame)
        self._create_command_section(app_frame)
        self._create_footer(app_frame)

    def _create_connection_section(self, parent: ttk.Frame) -> None:
        connection_frame = ttk.LabelFrame(
            parent,
            text="  Connection  ",
            style="Card.TLabelframe",
            padding=14,
        )
        connection_frame.grid(row=1, column=0, sticky="ew", pady=(0, 10))
        connection_frame.columnconfigure(1, weight=1)
        connection_frame.columnconfigure(5, weight=1)

        ttk.Label(connection_frame, text="COM port", style="Card.TLabel").grid(
            row=0, column=0, sticky="w", padx=(0, 8)
        )
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(
            connection_frame,
            textvariable=self.port_var,
            state="readonly",
            width=18,
        )
        self.port_combo.grid(row=0, column=1, padx=(0, 6))

        self.refresh_button = ttk.Button(
            connection_frame,
            text="↻  Refresh",
            style="Secondary.TButton",
            command=self._refresh_ports,
        )
        self.refresh_button.grid(row=0, column=2, padx=(0, 24))

        ttk.Label(connection_frame, text="Baud rate", style="Card.TLabel").grid(
            row=0, column=3, sticky="w", padx=(0, 8)
        )
        self.baud_var = tk.StringVar(value=str(DEFAULT_BAUD_RATE))
        self.baud_combo = ttk.Combobox(
            connection_frame,
            textvariable=self.baud_var,
            values=tuple(str(rate) for rate in self.BAUD_RATES),
            state="readonly",
            width=10,
        )
        self.baud_combo.grid(row=0, column=4, padx=(0, 24))

        self.connect_button = ttk.Button(
            connection_frame,
            text="Connect",
            style="Primary.TButton",
            command=self._toggle_connection,
        )
        self.connect_button.grid(row=0, column=5, sticky="e", padx=(0, 8))
        ttk.Label(
            connection_frame,
            text="Choose a port and baud rate before connecting.",
            style="Muted.TLabel",
        ).grid(row=1, column=0, columnspan=6, sticky="w", pady=(10, 0))

    def _create_diagnostics_section(self, parent: ttk.Frame) -> None:
        """Create protocol action controls with replaceable callbacks."""
        self.diagnostics_panel = DiagnosticsPanel(
            parent,
            did_options=self.DID_OPTIONS,
            on_request_seed=self.request_seed,
            on_send_key=self.send_key,
            on_read_did=self.read_did,
            on_write_did=self.write_did,
        )
        self.diagnostics_panel.frame.grid(
            row=2,
            column=0,
            sticky="ew",
            pady=(0, 10),
        )
        self.diagnostics_panel.set_connected(False)

    def _create_log_section(self, parent: ttk.Frame) -> None:
        log_frame = ttk.LabelFrame(
            parent,
            text="  Activity log  ",
            style="Card.TLabelframe",
            padding=10,
        )
        log_frame.grid(row=3, column=0, sticky="nsew", pady=(0, 10))
        log_frame.columnconfigure(0, weight=1)
        log_frame.rowconfigure(1, weight=1)

        toolbar = ttk.Frame(log_frame, style="Card.TFrame")
        toolbar.grid(row=0, column=0, sticky="ew", pady=(0, 8))
        toolbar.columnconfigure(0, weight=1)
        self.log_count_var = tk.StringVar(value="0 messages")
        ttk.Label(toolbar, textvariable=self.log_count_var, style="Muted.TLabel").grid(
            row=0, column=0, sticky="w"
        )
        self.autoscroll_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(
            toolbar,
            text="Auto-scroll",
            variable=self.autoscroll_var,
            style="Card.TCheckbutton",
        ).grid(row=0, column=1, padx=(0, 12))
        self.copy_log_button = ttk.Button(
            toolbar,
            text="Copy",
            style="Secondary.TButton",
            command=self._copy_log,
        )
        self.copy_log_button.grid(row=0, column=2, padx=(0, 6))
        self.clear_log_button = ttk.Button(
            toolbar,
            text="Clear",
            style="Secondary.TButton",
            command=self._clear_log,
        )
        self.clear_log_button.grid(row=0, column=3)

        self.log_terminal = scrolledtext.ScrolledText(
            log_frame,
            wrap=tk.WORD,
            state="disabled",
            font=("Consolas", 10),
            background=self.colors["terminal"],
            foreground=self.colors["terminal_text"],
            insertbackground="#ffffff",
            selectbackground="#334155",
            relief="flat",
            borderwidth=0,
        )
        self.log_terminal.grid(row=1, column=0, sticky="nsew")
        self.log_terminal.tag_configure("error", foreground="#f48771")
        self.log_terminal.tag_configure("warning", foreground="#fcd34d")
        self.log_terminal.tag_configure("tx", foreground="#93c5fd")
        self.log_terminal.tag_configure("rx", foreground="#86efac")

    def _create_command_section(self, parent: ttk.Frame) -> None:
        command_frame = ttk.LabelFrame(
            parent,
            text="  Send command  ",
            style="Card.TLabelframe",
            padding=14,
        )
        command_frame.grid(row=4, column=0, sticky="ew", pady=(0, 10))
        command_frame.columnconfigure(0, weight=1)

        ttk.Label(
            command_frame,
            text=(
                "Enter a command. Select String or Hex; frames are added "
                "automatically: "
                f"{START_OF_FRAME.hex(' ').upper()} ... "
                f"{END_OF_FRAME.hex(' ').upper()}."
            ),
            style="Muted.TLabel",
        ).grid(row=0, column=0, columnspan=6, sticky="w", pady=(0, 8))

        self.input_var = tk.StringVar()
        self.input_entry = ttk.Entry(
            command_frame,
            textvariable=self.input_var,
            font=("Segoe UI", 11),
        )
        self.input_entry.grid(row=1, column=0, sticky="ew", padx=(0, 8))
        self.input_entry.bind("<Return>", self._on_enter_pressed)
        self.input_entry.bind("<Up>", self._history_up)
        self.input_entry.bind("<Down>", self._history_down)

        ttk.Label(
            command_frame,
            text="Data format (terminal + services)",
            style="Card.TLabel",
        ).grid(
            row=1, column=1, sticky="e", padx=(0, 8)
        )
        self.data_format_var = tk.StringVar(value=DEFAULT_DATA_FORMAT)
        self.data_format_combo = ttk.Combobox(
            command_frame,
            textvariable=self.data_format_var,
            values=DATA_FORMATS,
            state="readonly",
            width=8,
        )
        self.data_format_combo.grid(row=1, column=2, padx=(0, 16))
        self.data_format_combo.bind(
            "<<ComboboxSelected>>",
            self._on_data_format_changed,
        )

        ttk.Label(command_frame, text="Line ending", style="Card.TLabel").grid(
            row=1, column=3, sticky="e", padx=(0, 8)
        )
        self.line_ending_var = tk.StringVar(value=DEFAULT_LINE_ENDING)
        self.line_ending_combo = ttk.Combobox(
            command_frame,
            textvariable=self.line_ending_var,
            values=tuple(self.LINE_ENDINGS),
            state="readonly",
            width=12,
        )
        self.line_ending_combo.grid(row=1, column=4, padx=(0, 8))

        self.send_button = ttk.Button(
            command_frame,
            text="Send  ↵",
            style="Primary.TButton",
            command=self._send_command,
            state="disabled",
        )
        self.send_button.grid(row=1, column=5)
        self._on_data_format_changed()

    def _create_footer(self, parent: ttk.Frame) -> None:
        footer = ttk.Frame(parent, style="App.TFrame")
        footer.grid(row=5, column=0, sticky="ew")
        footer.columnconfigure(0, weight=1)
        ttk.Label(
            footer,
            text="Logs are saved to bea_diag_checker.log",
            style="Footer.TLabel",
        ).grid(row=0, column=0, sticky="w")
        ttk.Label(
            footer,
            text=f"v{self.APP_VERSION}  •  Python {self.REQUIRED_PYTHON}",
            style="Footer.TLabel",
        ).grid(row=0, column=1, sticky="e")

    def _poll_log_queue(self) -> None:
        """Copy queued log messages into the Tk text widget on the UI thread."""
        try:
            while True:
                self._append_log(self.log_queue.get_nowait())
        except queue.Empty:
            pass
        if not self._closing:
            self.root.after(50, self._poll_log_queue)

    def _append_log(self, message: str) -> None:
        self.log_message_count += 1
        word = "message" if self.log_message_count == 1 else "messages"
        self.log_count_var.set(f"{self.log_message_count} {word}")

        self.log_terminal.configure(state="normal")
        tag = None
        if " ERROR:" in message:
            tag = "error"
        elif " WARNING:" in message:
            tag = "warning"
        elif " INFO: TX:" in message:
            tag = "tx"
        elif " INFO: RX:" in message:
            tag = "rx"
        if tag:
            self.log_terminal.insert(tk.END, message + "\n", tag)
        else:
            self.log_terminal.insert(tk.END, message + "\n")

        # Tự động trích xuất SEED và tính toán KEY khi nhận được phản hồi 67 01
        if "RX: [Hex]" in message and "67 01" in message:
            try:
                parts = message.split("RX: [Hex]")[-1].strip().split()
                if "67" in parts and "01" in parts:
                    idx = parts.index("67")
                    if idx + 1 < len(parts) and parts[idx + 1] == "01":
                        seed_hex = parts[idx + 2 : idx + 6]
                        if len(seed_hex) == 4:
                            seed = [int(b, 16) for b in seed_hex]
                            k0 = seed[0] ^ seed[1]
                            k1 = (seed[1] + seed[2]) & 0xFF
                            k2 = seed[2] ^ seed[3]
                            k3 = (seed[3] + seed[0]) & 0xFF
                            self._last_calculated_key = bytes([k0, k1, k2, k3])
            except Exception:
                pass

        if self.autoscroll_var.get():
            self.log_terminal.see(tk.END)
        self.log_terminal.configure(state="disabled")

    def _clear_log(self) -> None:
        self.log_terminal.configure(state="normal")
        self.log_terminal.delete("1.0", tk.END)
        self.log_terminal.configure(state="disabled")
        self.log_message_count = 0
        self.log_count_var.set("0 messages")

    def _copy_log(self) -> None:
        """Copy the visible terminal contents to the system clipboard."""
        contents = self.log_terminal.get("1.0", tk.END).rstrip()
        if not contents:
            return
        self.root.clipboard_clear()
        self.root.clipboard_append(contents)
        self.root.update_idletasks()

    def _refresh_ports(self) -> None:
        """Refresh the COM-port selector from ports detected by pyserial."""
        try:
            ports = sorted(list_ports.comports(), key=lambda port: port.device)
            port_names = [port.device for port in ports]
        except (OSError, serial.SerialException) as exc:
            self.logger.error("Could not enumerate COM ports: %s", exc)
            return

        self.port_combo["values"] = port_names
        if self.port_var.get() not in port_names:
            self.port_var.set(port_names[0] if port_names else "")
        if port_names:
            self.logger.info("Available COM ports: %s", ", ".join(port_names))
        else:
            self.logger.warning("No COM ports detected.")

    def _toggle_connection(self) -> None:
        if self.serial_manager.is_connected:
            self._disconnect()
        else:
            self._connect()

    def _connect(self) -> None:
        port = self.port_var.get()
        if not port:
            self._set_status("Error", "Error.Status.TLabel")
            self.logger.warning("Select a COM port before connecting.")
            return

        try:
            baud_rate = int(self.baud_var.get())
            self.serial_manager.connect(port, baud_rate)
        except (ValueError, SerialConnectionError) as exc:
            self._set_status("Error", "Error.Status.TLabel")
            self.logger.error("Connection failed: %s", exc)
            return

        self._set_connection_controls(connected=True)
        self._set_status("Connected", "Connected.Status.TLabel")
        self.input_entry.focus_set()

    def _disconnect(self) -> None:
        self.serial_manager.disconnect()
        self._set_connection_controls(connected=False)
        self._set_status("Disconnected", "Disconnected.Status.TLabel")

    def _set_connection_controls(self, *, connected: bool) -> None:
        if connected:
            self.connect_button.configure(text="Disconnect")
            self.send_button.configure(state="normal")
            self.port_combo.configure(state="disabled")
            self.baud_combo.configure(state="disabled")
            self.refresh_button.configure(state="disabled")
        else:
            self.connect_button.configure(text="Connect")
            self.send_button.configure(state="disabled")
            self.port_combo.configure(state="readonly")
            self.baud_combo.configure(state="readonly")
            self.refresh_button.configure(state="normal")
        self.diagnostics_panel.set_connected(connected)

    def _set_status(self, text: str, style: str) -> None:
        self.status_label.configure(text=f"●  {text}", style=style)

    def _schedule_connection_lost(self) -> None:
        """Schedule UI updates safely when the reader thread loses the port."""
        if self._closing:
            return
        try:
            self.root.after(0, self._handle_connection_lost)
        except tk.TclError:
            # The window may be closing while the reader thread reports an error.
            pass

    def _handle_connection_lost(self) -> None:
        if self._closing or self.serial_manager.is_connected:
            return
        self._set_connection_controls(connected=False)
        self._set_status("Connection lost", "Error.Status.TLabel")

    def _on_enter_pressed(self, _event: tk.Event) -> str:
        self._send_command()
        return "break"

    def _history_up(self, _event: tk.Event) -> str:
        if not self.command_history:
            return "break"
        if self.history_index is None:
            self.history_index = len(self.command_history)
        self.history_index = max(0, self.history_index - 1)
        self._restore_history_item(self.command_history[self.history_index])
        self.input_entry.icursor(tk.END)
        return "break"

    def _history_down(self, _event: tk.Event) -> str:
        if self.history_index is None:
            return "break"
        self.history_index += 1
        if self.history_index >= len(self.command_history):
            self.history_index = None
            self.input_var.set("")
        else:
            self._restore_history_item(self.command_history[self.history_index])
        self.input_entry.icursor(tk.END)
        return "break"

    def _restore_history_item(self, item: CommandHistoryItem) -> None:
        """Restore command text and the format options used with it."""
        self.input_var.set(item.input_data)
        self.data_format_var.set(item.data_format)
        self.line_ending_var.set(item.line_ending)
        self._on_data_format_changed()

    def _send_command(self) -> None:
        command = self.input_var.get()
        if self.process(command):
            history_item = CommandHistoryItem(
                input_data=command,
                data_format=self.data_format_var.get(),
                line_ending=self.line_ending_var.get(),
            )
            if not self.command_history or self.command_history[-1] != history_item:
                self.command_history.append(history_item)
                self.command_history = self.command_history[-COMMAND_HISTORY_LIMIT:]
            self.history_index = None
            self.input_var.set("")

    def _on_data_format_changed(self, _event: tk.Event | None = None) -> None:
        """Enable line endings only for String payloads."""
        if self.data_format_var.get() == "Hex":
            self.line_ending_combo.configure(state="disabled")
        else:
            self.line_ending_combo.configure(state="readonly")

    def process(self, input_data: str) -> bool:
        """Process a String or Hex command and send a framed payload."""
        line_ending = self.LINE_ENDINGS.get(self.line_ending_var.get(), "\r\n")
        try:
            if self.data_format_var.get() == "Hex":
                self.serial_manager.send_hex(input_data)
            else:
                self.serial_manager.send_text(input_data, line_ending)
        except EmptyCommandError as exc:
            self.logger.warning("%s", exc)
            return False
        except InvalidPayloadError as exc:
            self.logger.warning("%s", exc)
            return False
        except NotConnectedError as exc:
            self.logger.warning("%s", exc)
            return False
        except SerialWriteError as exc:
            self.logger.error("Serial write failed: %s", exc)
            return False
        return True

    def request_seed(self) -> None:
        """Reserved callback for the Security Access Request Seed action.

        Replace this method body with the BEA-specific seed request flow.
        """
        try:
            self.diagnostic_manager.process(
                "0x27",
                "0x01",
                data_format=self.data_format_var.get(),
            )
        except (DiagnosticManagerError, SerialManagerError) as exc:
            self.logger.error("Read DID failed: %s", exc)

    def send_key(self) -> None:
        """Send Key callback: calculates and sends key based on received seed."""
        if not self._last_calculated_key:
            self.logger.warning("No Seed received yet! Click 'Request Seed' first.")
            return

        try:
            req_data = bytes([0x02]) + self._last_calculated_key
            self.diagnostic_manager.process(
                "0x27",
                req_data,
                data_format=self.data_format_var.get(),
            )
            self.logger.info("Sent Key: %s", self._last_calculated_key.hex(" ").upper())
        except (DiagnosticManagerError, SerialManagerError) as exc:
            self.logger.error("Send Key failed: %s", exc)

    def read_did(self) -> None:
        """Build and send a UDS Read DID request through service 0x22.

        The selected DID is available through ``self.diagnostics_panel.read_did``.
        The response is received asynchronously by ``SerialManager`` and
        appears in the RX activity log.
        """
        
        requestdataFromUI = DiagnosticRequestFromUI(
            DID=self.diagnostics_panel.read_did,
            Data=None,
            DataLength=0
        )        
        try:
            self.diagnostic_manager.process(
                "0x22",
                requestdataFromUI,
                data_format=self.data_format_var.get(),
            )
        except (DiagnosticManagerError, SerialManagerError) as exc:
            self.logger.error("Read DID failed: %s", exc)

    def write_did(self) -> None:
        """Reserved callback for the Write DID action.

        The selected DID and string value are available through
        ``self.diagnostics_panel.write_did`` and
        ``self.diagnostics_panel.write_value``.  Replace this method body with
        the BEA-specific write flow.
        """
        try:
            data = parse_hex_bytes(
                self.diagnostics_panel.write_value,
                field_name="Data",
            )
        except InvalidDiagnosticRequestError as exc:
            self.logger.warning("Invalid Write DID data: %s", exc)
            return

        request_data = DiagnosticRequestFromUI(
                DID=self.diagnostics_panel.write_did,
                Data=data,
                DataLength=len(data),
        )

        try:
            self.diagnostic_manager.process(
                "0x2e",
                request_data,
                data_format=self.data_format_var.get(),
            )
            self.logger.info(
                "Write DID for %s (value length: %d).",
                self.diagnostics_panel.write_did or "no DID",
                len(self.diagnostics_panel.write_value) if self.diagnostics_panel.write_value else 0,
            )
        except (DiagnosticManagerError, SerialManagerError) as exc:
            self.logger.error("Write DID failed: %s", exc)
        

    def _on_close(self) -> None:
        self._closing = True
        self.serial_manager.disconnect(announce=False)
        for handler in (self.file_handler, self.ui_handler):
            self.logger.removeHandler(handler)
            handler.close()
        self.root.destroy()


__all__ = ["BEADiagChecker"]
