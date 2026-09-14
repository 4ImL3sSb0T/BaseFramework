"""STM32H750 electronic-load host application (PC side).

Talks to the firmware over USART1 at 115200 8N1. Two transports are available:
a real serial port and a built-in device simulator, so the whole UI can be
exercised without hardware.

See README.md for usage and PROTOCOL.md for the binary frame specification.
"""

from __future__ import annotations

__version__ = "0.1.0"

__all__ = ["__version__"]
