"""Entry point shim — delegates to loader_host.app.main.

Mirrors the scripts/map_analyzer/main.py convention in this repo.
"""

from __future__ import annotations

from loader_host.app import main

if __name__ == "__main__":
    raise SystemExit(main())
