"""Dispatcher for modular UDS diagnostic services."""

from __future__ import annotations

from collections.abc import Callable, Iterator
from dataclasses import dataclass
import importlib
import logging
from types import ModuleType
import pkgutil
from typing import Protocol, TypeAlias

from .exceptions import (
    DiagnosticManagerError,
    DiagnosticServiceError,
    DiagnosticServiceRegistrationError,
    SerialManagerError,
    UnknownDiagnosticServiceError,
)
from .serial_manager import SerialManager

ServiceIdentifier: TypeAlias = int | str
ServiceProcessor: TypeAlias = Callable[
    [SerialManager, object, logging.Logger, str], bytes
]


class DiagnosticService(Protocol):
    """Interface required from a ``serviceID_serviceName.py`` module."""

    SERVICE_ID: ServiceIdentifier
    SERVICE_NAME: str

    def process(
        self,
        serial_manager: SerialManager,
        request_data: object,
        logger: logging.Logger,
        data_format: str,
    ) -> bytes:
        """Build and send one request for this diagnostic service."""
        ...


@dataclass(frozen=True)
class RegisteredDiagnosticService:
    """Validated service metadata held by :class:`DiagnosticManager`."""

    service_id: str
    service_name: str
    processor: ServiceProcessor
    module_name: str
    
@dataclass(frozen=True)
class DiagnosticRequestFromUI:
    """Validated request from UI metadata held by :class:`DiagnosticManager`."""

    DID: str
    Data: str
    DataLength: bytes


class DiagnosticManager:
    """Discover, register, and dispatch modular diagnostic services.

    Service modules are kept in ``core/Diagnostic`` and follow the naming
    convention ``serviceID_serviceName.py``.  Each module exports
    ``SERVICE_ID``, ``SERVICE_NAME``, and a ``process`` function accepting the
    serial manager, request data, logger, and selected data format. Built-in
    modules are discovered automatically; additional modules can be registered
    with :meth:`register_service` without changing this class.
    """

    def __init__(
        self,
        serial_manager: SerialManager,
        logger: logging.Logger,
        *,
        auto_discover: bool = True,
    ) -> None:
        self._serial_manager = serial_manager
        self._logger = logger
        self._services: dict[str, RegisteredDiagnosticService] = {}
        if auto_discover:
            self.discover_services()

    @property
    def serial_manager(self) -> SerialManager:
        """Return the serial transport used by all registered services."""
        return self._serial_manager

    @staticmethod
    def normalize_service_id(service_id: ServiceIdentifier) -> str:
        """Normalize integer or hexadecimal service IDs to ``0xNN``."""
        if isinstance(service_id, bool):
            raise DiagnosticServiceRegistrationError(
                "A service ID must be an integer or hexadecimal string."
            )

        if isinstance(service_id, int):
            numeric_id = service_id
        elif isinstance(service_id, str):
            value = service_id.strip()
            if value.lower().startswith("0x"):
                value = value[2:]
            if not value:
                raise DiagnosticServiceRegistrationError("A service ID cannot be empty.")
            try:
                numeric_id = int(value, 16)
            except ValueError as exc:
                raise DiagnosticServiceRegistrationError(
                    f"Invalid diagnostic service ID: {service_id!r}."
                ) from exc
        else:
            raise DiagnosticServiceRegistrationError(
                "A service ID must be an integer or hexadecimal string."
            )

        if not 0 <= numeric_id <= 0xFF:
            raise DiagnosticServiceRegistrationError(
                f"Diagnostic service ID must fit in one byte: {numeric_id!r}."
            )
        return f"0x{numeric_id:02X}"

    def register_service(self, service: ModuleType | DiagnosticService) -> None:
        """Validate and register one service module or compatible object."""
        raw_service_id = getattr(service, "SERVICE_ID", None)
        raw_service_name = getattr(service, "SERVICE_NAME", None)
        processor = getattr(service, "process", None)

        if raw_service_id is None:
            raise DiagnosticServiceRegistrationError(
                f"{service!r} does not define SERVICE_ID."
            )
        if not isinstance(raw_service_name, str) or not raw_service_name.strip():
            raise DiagnosticServiceRegistrationError(
                f"{service!r} must define a non-empty SERVICE_NAME."
            )
        if not callable(processor):
            raise DiagnosticServiceRegistrationError(
                f"{service!r} must define a callable process function."
            )

        service_id = self.normalize_service_id(raw_service_id)
        registered = RegisteredDiagnosticService(
            service_id=service_id,
            service_name=raw_service_name.strip(),
            processor=processor,
            module_name=getattr(service, "__name__", service.__class__.__name__),
        )
        if service_id in self._services:
            previous = self._services[service_id]
            self._logger.debug(
                "Replacing diagnostic service %s (%s) with %s.",
                service_id,
                previous.module_name,
                registered.module_name,
            )
        self._services[service_id] = registered
        self._logger.debug(
            "Registered diagnostic service %s - %s.",
            service_id,
            registered.service_name,
        )

    def discover_services(self) -> tuple[str, ...]:
        """Discover valid service modules in the built-in Diagnostic package.

        Invalid or helper modules without the required service interface are
        skipped and logged.  Returning the discovered IDs makes startup and
        tests observable without exposing the internal registry.
        """
        package = importlib.import_module(f"{__package__}.Diagnostic")
        discovered: list[str] = []
        package_path = getattr(package, "__path__", ())
        for module_info in pkgutil.iter_modules(package_path):
            if module_info.name.startswith("_"):
                continue
            module_name = f"{package.__name__}.{module_info.name}"
            module = importlib.import_module(module_name)
            if not self._is_service_module(module):
                self._logger.debug("Skipping non-service module %s.", module_name)
                continue
            self.register_service(module)
            discovered.append(self.normalize_service_id(module.SERVICE_ID))
        return tuple(sorted(discovered))

    @staticmethod
    def _is_service_module(module: ModuleType) -> bool:
        return (
            hasattr(module, "SERVICE_ID")
            and isinstance(getattr(module, "SERVICE_NAME", None), str)
            and callable(getattr(module, "process", None))
        )

    def get_service(self, service_id: ServiceIdentifier) -> RegisteredDiagnosticService:
        """Return a registered service or raise an explicit lookup error."""
        normalized_id = self.normalize_service_id(service_id)
        try:
            return self._services[normalized_id]
        except KeyError as exc:
            available = ", ".join(self.list_services()) or "none"
            raise UnknownDiagnosticServiceError(
                f"Unknown diagnostic service {normalized_id}. Available: {available}."
            ) from exc

    def process(
        self,
        service_id: ServiceIdentifier,
        request_data: DiagnosticRequestFromUI,
        *,
        data_format: str = "Hex",
    ) -> bytes:
        """Dispatch ``request_data`` to the service identified by ``service_id``.

        Each service builds its own protocol request and uses the shared
        :class:`SerialManager` to send it.  The return value is the exact bytes
        written to the serial port; incoming device responses continue through
        the serial manager's asynchronous RX logging path.
        """
        service = self.get_service(service_id)
        self._logger.debug(
            "Processing diagnostic service %s (%s).",
            service.service_id,
            service.service_name,
        )
        try:
            payload = service.processor(
                self._serial_manager,
                request_data,
                self._logger,
                data_format,
            )
        except (SerialManagerError, DiagnosticManagerError):
            raise
        except Exception as exc:
            self._logger.exception(
                "Diagnostic service %s failed.",
                service.service_id,
            )
            raise DiagnosticServiceError(
                f"Diagnostic service {service.service_id} failed: {exc}"
            ) from exc

        if not isinstance(payload, bytes):
            raise DiagnosticServiceError(
                f"Diagnostic service {service.service_id} returned "
                f"{type(payload).__name__}, expected bytes."
            )
        return payload

    def list_services(self) -> dict[str, str]:
        """Return registered service IDs mapped to human-readable names."""
        return {
            service_id: self._services[service_id].service_name
            for service_id in sorted(self._services)
        }

    def iter_services(self) -> Iterator[RegisteredDiagnosticService]:
        """Iterate over registered services in service-ID order."""
        for service_id in sorted(self._services):
            yield self._services[service_id]


__all__ = [
    "DiagnosticManager",
    "DiagnosticService",
    "RegisteredDiagnosticService",
    "ServiceIdentifier",
]
