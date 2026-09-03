"""Exceptions raised by the serial communication service."""


class SerialManagerError(RuntimeError):
    """Base class for serial manager failures."""


class SerialConnectionError(SerialManagerError):
    """The serial port could not be opened."""


class SerialWriteError(SerialManagerError):
    """A command could not be written to the serial port."""


class NotConnectedError(SerialManagerError):
    """A serial operation was requested without an open connection."""


class EmptyCommandError(ValueError):
    """The command contains no non-whitespace characters."""


class InvalidPayloadError(ValueError):
    """The generic serial command payload is invalid."""


class DiagnosticManagerError(RuntimeError):
    """Base class for diagnostic service dispatch failures."""


class UnknownDiagnosticServiceError(DiagnosticManagerError):
    """No registered service matches the requested service identifier."""


class DiagnosticServiceRegistrationError(DiagnosticManagerError):
    """A service module does not implement the required interface."""


class InvalidDiagnosticRequestError(DiagnosticManagerError):
    """The input for a diagnostic service is invalid."""


class DiagnosticServiceError(DiagnosticManagerError):
    """A registered service failed while building or sending a request."""
