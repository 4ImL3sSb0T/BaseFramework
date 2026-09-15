"""Offscreen smoke test: build the main window and flip the theme both ways."""

import os

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtWidgets import QApplication  # noqa: E402

from loader_host import theme  # noqa: E402
from loader_host.main_window import MainWindow  # noqa: E402

app = QApplication([])
theme.apply(app, "light")
window = MainWindow(start_sim=False)
assert theme.current_theme() == "light"
assert theme.BG == "#e6e9ef"

window._on_theme_toggled()
assert theme.current_theme() == "dark"
assert theme.BG == "#181825"

window._on_theme_toggled()
assert theme.current_theme() == "light"

theme.set_theme("dark")
theme.set_theme("light")
try:
    theme.set_theme("nope")
except ValueError:
    pass
else:
    raise SystemExit("set_theme should reject unknown names")

print("SMOKE OK")

# Avoid "QThread destroyed while still running" at interpreter teardown.
window._client.shutdown()
