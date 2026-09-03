# BEA Diag Checker

BEA Diag Checker is a Windows desktop application for communicating with a BEA device over a serial COM port.

## Features

- Select and refresh available COM ports.
- Select a baud rate.
- Connect and disconnect from the serial device.
- Use the Security Access area with reserved **Request Seed** and **Send Key** callbacks.
- Select a DID from the configured list and use the Read DID control.
- Select a DID, enter a string value, and use the Write DID control.
- Dispatch modular UDS services from the `DiagnosticManager`.
- View received serial data in the logging terminal.
- Select **String** or **Hex** input for terminal commands and diagnostic service buttons.
- Send terminal data with the **Send** button or the **Enter** key.
- Automatically wrap every transmitted payload with the BEA protocol frame:
  - Start of Frame: `0F FF F0`
  - End of Frame: `F0 00 0F`
- Hide the Start/End of Frame bytes from the TX/RX activity log.
- Review color-coded TX/RX messages and message count in the activity log.
- Copy or clear the activity log and turn auto-scroll on or off.
- Reuse recent commands with the Up and Down arrow keys.
- Select the line ending appended to a String command:
  - None
  - LF (`\\n`)
  - CRLF (`\\r\\n`), the default
- Save application logs to `bea_diag_checker.log` in the application folder.

## Requirements

- Windows 10 or later
- Python **3.12.13.1** at `C:\toolbase\python\3.12.13.1\python.exe`
- A USB-to-serial device and its Windows driver
- A working Python installation that includes Tcl/Tk

The Python standard library provides Tkinter. Tkinter is used for the graphical interface and does not need to be installed with pip.

## Setup on Windows

Open PowerShell in this folder:

```powershell
cd D:\01_sb\BEA_Diag_Checker
```

### 1. Verify Python

Verify the required interpreter:

```powershell
C:\toolbase\python\3.12.13.1\python.exe --version
```

The expected version is Python 3.12. Install the required Python distribution
if the executable is not available. The selected installation must include
the standard library and **Tcl/Tk support**.

### 2. Verify Tkinter

Run:

```powershell
C:\toolbase\python\3.12.13.1\python.exe -m tkinter
```

A small Tkinter window should open. Close the window after verifying it works.

If the command reports `Can't find a usable init.tcl`, the selected Python distribution does not contain Tcl/Tk. Install or select a complete Python distribution from python.org, then recreate the virtual environment.

### 3. Create a virtual environment

```powershell
C:\toolbase\python\3.12.13.1\python.exe -m venv .venv
```

Activate it:

```powershell
.\.venv\Scripts\Activate.ps1
```

If PowerShell blocks script execution, allow scripts for the current PowerShell session only:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\.venv\Scripts\Activate.ps1
```

### 4. Install dependencies

Upgrade pip and install the project dependencies:

```powershell
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
```

The only external dependency is `pyserial`.

## Run the application

With the virtual environment activated, run:

```powershell
python BEA_DiagChecker.py
```

The application window should open.

Alternatively, run directly with the required interpreter:

```powershell
C:\toolbase\python\3.12.13.1\python.exe BEA_DiagChecker.py
```

The virtual environment does not need to be activated if the interpreter is called directly:

```powershell
.\.venv\Scripts\python.exe BEA_DiagChecker.py
```

When multiple Python installations are present, select the Python 3.12
interpreter that contains the installed `pyserial` package in VS Code
(`Python: Select Interpreter`).
This workspace includes [`.vscode/settings.json`](.vscode/settings.json) with
the required interpreter path.

## Using the application

1. Connect the BEA device to the computer.
2. Start the application.
3. Click **Refresh** to update the COM-port list.
4. Select the device COM port.
5. Select the baud rate required by the device. The default is `115200`.
6. Click **Connect**.
7. Type a command in the input field.
8. Select **String** or **Hex** in **Data format (terminal + services)**.
9. For String data, select the required line ending. The default is CRLF.
10. For Hex data, enter values such as `22 F1 80` or `0x22 0xF1 0x80`.
11. Click **Send** or press **Enter**.
12. Monitor transmitted commands (`TX`) and received data (`RX`) in the logging terminal.
13. Select a DID and use **Read** or select a DID, enter a string value, and use **Write**.
14. Use **Request Seed** and **Send Key** after implementing their reserved callbacks.
15. Click **Disconnect** before changing the COM port or baud rate.

The **Send** button is disabled until a serial connection is established.
Diagnostic action buttons are also disabled until a serial connection is
established. The DID selectors remain available for preparation.

The selected data format applies to the Read DID and future diagnostic button
implementations as well as the generic terminal. For example, Read DID
`0xF180` sends:

- Hex mode: binary payload `22 F1 80`.
- String mode: ASCII payload `22F180`.

Both payloads are still wrapped with the BEA frame markers before transmission.

## Diagnostic callback extension points

The four diagnostic buttons are intentionally UI-only placeholders. Replace
the method bodies in [bea_diag_checker/ui/main_window.py](bea_diag_checker/ui/main_window.py)
when the BEA-specific protocol is available:

- `request_seed()` - **Request Seed** callback
- `send_key()` - **Send Key** callback
- `read_did()` - **Read** callback; selected value is available from `self.diagnostics_panel.read_did`
- `write_did()` - **Write** callback; selected DID and string value are available from `self.diagnostics_panel.write_did` and `self.diagnostics_panel.write_value`

The initial DID selector values are centralized in
[bea_diag_checker/config/constants.py](bea_diag_checker/config/constants.py)
as `DID_OPTIONS`. Replace or extend that tuple with the device's actual DID
database without changing the UI layout.

## Diagnostic service architecture

[bea_diag_checker/core/diagnostic_manager.py](bea_diag_checker/core/diagnostic_manager.py)
automatically discovers service modules in
[bea_diag_checker/core/Diagnostic](bea_diag_checker/core/Diagnostic). Each
service follows the `serviceID_serviceName.py` convention and exports:

- `SERVICE_ID` - one-byte integer service ID, such as `0x22`.
- `SERVICE_NAME` - display name, such as `read_DID`.
- `process(serial_manager, request_data, logger, data_format)` - validates
  the service input, builds the request, and sends it through `SerialManager`
  in the selected String or Hex format.

The built-in services are:

- [0x22_read_DID.py](bea_diag_checker/core/Diagnostic/0x22_read_DID.py) -
  accepts a two-byte DID such as `0xF180` and sends the binary UDS request
  `22 F1 80` inside the configured BEA frame.
- [0x27_security_access.py](bea_diag_checker/core/Diagnostic/0x27_security_access.py) -
  supports Request Seed and Send Key sub-functions through a common
  `process()` method, plus `request_seed()` and `send_key()` helpers.

The serial manager's `send_bytes()` method is used for UDS requests so binary
service IDs and data are transmitted unchanged inside the BEA frame. Device
responses continue to be received asynchronously and binary responses are
shown as hexadecimal data in the RX activity log.

To add another service, create a module in the Diagnostic folder with the
three exports above. The manager will discover it at startup. Because the
requested filenames begin with `0x`, they are loaded with `importlib`; normal
Python `from ... import ...` syntax cannot import a module whose filename
starts with a digit.

## Serial behavior

The `process()` function validates the selected input mode:

- **String** mode encodes the input as UTF-8 and appends the selected line ending.
- **Hex** mode accepts hexadecimal digits with optional `0x` prefixes and common separators.

The transport then writes:

`Start of Frame + payload + End of Frame`

For example, String command `ABC` with no line ending is transmitted as:
`0F FF F0 41 42 43 F0 00 0F`.

Incoming data is read by a background thread so the user interface remains
responsive. Printable responses are logged as String data; binary responses
are logged as Hex data. The frame markers are removed before either TX or RX
data is displayed in the activity log.

## Troubleshooting

### No COM port is listed

- Confirm that the device is connected.
- Install the USB-to-serial driver supplied by the device manufacturer.
- Check **Device Manager > Ports (COM & LPT)** in Windows.
- Click **Refresh** after connecting the device.

### Connection fails

- Confirm that the selected COM port is correct.
- Close other applications that may be using the port, such as a terminal emulator.
- Confirm that the baud rate matches the BEA device configuration.
- Disconnect and reconnect the device, then refresh the port list.

### The Send button is disabled

The application must be connected to a COM port before commands can be sent.

### Tkinter or `init.tcl` error

The Python installation is missing Tcl/Tk or its Tcl/Tk paths are invalid. Install Python using the official Windows installer with the standard library and Tcl/Tk components enabled. Then remove and recreate `.venv` and repeat the setup steps.

## Project structure

- [BEA_DiagChecker.py](BEA_DiagChecker.py) - Backward-compatible launcher
- [.vscode/settings.json](.vscode/settings.json) - Required VS Code Python interpreter
- [bea_diag_checker/main.py](bea_diag_checker/main.py) - Application entry point
- [bea_diag_checker/ui/main_window.py](bea_diag_checker/ui/main_window.py) - Tkinter UI and user interaction
- [bea_diag_checker/ui/diagnostics_panel.py](bea_diag_checker/ui/diagnostics_panel.py) - Security Access and DID controls
- [bea_diag_checker/core/serial_manager.py](bea_diag_checker/core/serial_manager.py) - UI-independent serial service
- [bea_diag_checker/core/framing.py](bea_diag_checker/core/framing.py) - BEA start/end frame handling
- [bea_diag_checker/core/data_codec.py](bea_diag_checker/core/data_codec.py) - Generic Hex input parsing
- [bea_diag_checker/core/diagnostic_manager.py](bea_diag_checker/core/diagnostic_manager.py) - Diagnostic service discovery and dispatch
- [bea_diag_checker/core/Diagnostic/0x22_read_DID.py](bea_diag_checker/core/Diagnostic/0x22_read_DID.py) - Read DID service implementation
- [bea_diag_checker/core/Diagnostic/0x27_security_access.py](bea_diag_checker/core/Diagnostic/0x27_security_access.py) - Security Access service implementation
- [bea_diag_checker/core/exceptions.py](bea_diag_checker/core/exceptions.py) - Serial domain exceptions
- [bea_diag_checker/config/constants.py](bea_diag_checker/config/constants.py) - Centralized application settings
- [bea_diag_checker/utils/logging_utils.py](bea_diag_checker/utils/logging_utils.py) - File and UI logging
- [tests/test_serial_manager.py](tests/test_serial_manager.py) - Automated serial-service tests
- [tests/test_diagnostic_manager.py](tests/test_diagnostic_manager.py) - Diagnostic dispatch and service tests
- [requirements.txt](requirements.txt) - Runtime dependency
- [promt.md](promt.md) - Original application requirements
- [bea_diag_checker.log](bea_diag_checker.log) - Runtime log file created when the application starts

The code is split into UI, core, configuration, and utility modules so the
serial behavior can be tested without starting Tkinter or requiring hardware.

## Run tests

Install the development dependency and run the test suite:

```powershell
python -m pip install pytest
python -m pytest
```

The tests use a fake serial connection and do not require a physical COM port.
