"""Offscreen screenshots of both themes for visual verification."""

import os

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtWidgets import QApplication  # noqa: E402

from loader_host import theme  # noqa: E402
from loader_host.main_window import MainWindow  # noqa: E402

app = QApplication([])
theme.apply(app, "light")
window = MainWindow(start_sim=False)
window.resize(1440, 900)
window.show()
app.processEvents()
window.grab().save("_shot_light.png")

window._on_theme_toggled()
app.processEvents()
window.grab().save("_shot_dark.png")

print("SHOTS OK")
window._client.shutdown()
