"""UDS diagnostic service modules.

Each service module follows the ``serviceID_serviceName.py`` convention and
exports these members:

* ``SERVICE_ID`` - integer UDS service identifier, for example ``0x22``.
* ``SERVICE_NAME`` - human-readable service name.
* ``process(serial_manager, request_data, logger)`` - builds and sends the
  service request.

The filenames in this directory intentionally retain the requested
``0xNN_serviceName.py`` format.  ``DiagnosticManager`` loads them with
``importlib`` because a filename beginning with a digit cannot be imported
with normal ``from ... import ...`` syntax.
"""

__all__: list[str] = []
