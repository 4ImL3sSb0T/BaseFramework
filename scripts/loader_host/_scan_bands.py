"""Scan horizontal pixel runs in a screenshot to locate the stray bands."""

import sys

from PySide6.QtGui import QImage

path = sys.argv[1]
ys = [int(v) for v in sys.argv[2].split(",")]

img = QImage(path)
print(f"# {path}  {img.width()}x{img.height()}")
for y in ys:
    runs = []
    prev = None
    start = 0
    for x in range(img.width()):
        c = img.pixelColor(x, y).name()
        if c != prev:
            if prev is not None and x - start >= 4:
                runs.append((start, x - 1, prev))
            prev = c
            start = x
    if img.width() - start >= 4:
        runs.append((start, img.width() - 1, prev))
    print(f"y={y}: " + "  ".join(f"[{a}-{b}] {c}" for a, b, c in runs))
