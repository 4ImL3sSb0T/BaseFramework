"""Real-font before/after crops of the 限值 card.

Renders with the native platform plugin (so system fonts are used) but with
WA_DontShowOnScreen, so no window is ever mapped.  The "before" state is
recreated by appending the pre-fix rule (a blanket QSS background on QLabel).
"""

from PySide6.QtCore import Qt
from PySide6.QtGui import QGuiApplication, QImage, QPainter
from PySide6.QtWidgets import QApplication

from loader_host import theme
from loader_host.main_window import MainWindow

BOX = (1110, 228, 330, 140)  # x, y, w, h -- the 限值 card in the right column
SIZE = (1440, 900)


def render(name: str) -> None:
    app = QApplication.instance() or QApplication([])
    theme.apply(app, name)
    window = MainWindow()
    window.resize(*SIZE)
    window.setAttribute(Qt.WA_DontShowOnScreen, True)
    window.show()
    app.processEvents()

    window.grab().save(f"_native_after_{name}.png")

    # Pre-fix look: the universal QWidget rule also hit QLabel.
    app.setStyleSheet(theme.stylesheet() + f"\nQLabel {{ background: {theme.BG}; }}")
    app.processEvents()
    window.grab().save(f"_native_before_{name}.png")

    window._client.shutdown()
    window.close()


def stack(*images: QImage) -> QImage:
    gap = 6
    out = QImage(
        max(i.width() for i in images) + 8,
        sum(i.height() for i in images) + gap * (len(images) - 1) + 8,
        QImage.Format_RGB32,
    )
    out.fill(0xFF9AA0A6)
    painter = QPainter(out)
    y = 4
    for image in images:
        painter.drawImage(4, y, image)
        y += image.height() + gap
    painter.end()
    return out


for name in ("light", "dark"):
    render(name)
    shot = QImage(f"_native_before_{name}.png")
    dpr = shot.width() / SIZE[0]  # display scaling: 1.25 on this machine
    box = tuple(int(v * dpr) for v in BOX)
    print(name, "dpr", dpr, "size", shot.size(), "box", box)
    before = shot.copy(*box)
    after = QImage(f"_native_after_{name}.png").copy(*box)
    stack(before, after).save(f"_fix_cmp_{name}.png")
    print(name, "OK")

print("CROPS OK")
