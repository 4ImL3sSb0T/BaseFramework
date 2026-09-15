"""Reusable UI widgets: readout cards, status banner, charts, terminal.

Kept separate from `main_window` so layout and behaviour can be reasoned about
independently, and so the window file stays readable.
"""

from __future__ import annotations

from collections import deque

import pyqtgraph as pg
from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QFont, QTextCursor
from PySide6.QtWidgets import (
    QCheckBox,
    QFrame,
    QHBoxLayout,
    QLabel,
    QPlainTextEdit,
    QSizePolicy,
    QVBoxLayout,
    QWidget,
)

from . import config, theme
from .model import LoaderSnapshot

# ---------------------------------------------------------------------------
# Card container
# ---------------------------------------------------------------------------


class Card(QFrame):
    """Flat card container with an uppercase section title.

    Replaces QGroupBox for a cleaner, more modern look: no notched border,
    just a rounded panel with a small muted title inside. Add content to the
    exposed ``body`` layout.
    """

    def __init__(self, title: str, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("Card")
        self.body = QVBoxLayout(self)
        self.body.setContentsMargins(14, 12, 14, 14)
        self.body.setSpacing(10)
        label = QLabel(title.upper())
        label.setObjectName("CardTitle")
        self.body.addWidget(label)


# ---------------------------------------------------------------------------
# Readout card
# ---------------------------------------------------------------------------


class ReadoutCard(QFrame):
    """A large numeric readout: value, unit, label, optional sub-caption.

    The channel colour is applied to the *label*, not the value. Keeping the
    number neutral matters: a temperature readout that is permanently red
    (its channel colour) would make the amber/red near-limit warning
    indistinguishable from normal operation.
    """

    def __init__(
        self,
        label: str,
        unit: str,
        color: str = theme.FG,
        decimals: int = 3,
        parent: QWidget | None = None,
    ) -> None:
        super().__init__(parent)
        self.setObjectName("Readout")
        self.setMinimumWidth(120)
        self.setMinimumHeight(96)
        self._decimals = decimals
        self._accent = color
        self._unit = unit
        self._warn_high: float | None = None
        self._warn_ratio = 0.9
        self._last_value: float | None = None
        self._last_sub = ""

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 10, 12, 10)
        layout.setSpacing(3)

        self._label = QLabel(label.upper())
        self._label.setObjectName("ReadoutLabel")
        self._apply_accent(color)

        value_row = QHBoxLayout()
        value_row.setSpacing(4)
        self._value = QLabel("—")
        self._value.setObjectName("ReadoutValue")
        self._value.setStyleSheet(f"color: {theme.FG_DIM};")
        self._unit = QLabel(unit)
        self._unit.setObjectName("ReadoutUnit")
        self._unit.setAlignment(Qt.AlignBottom | Qt.AlignLeft)
        value_row.addWidget(self._value)
        value_row.addWidget(self._unit)
        value_row.addStretch(1)

        self._sub = QLabel("")
        self._sub.setObjectName("Hint")

        layout.addWidget(self._label)
        layout.addLayout(value_row)
        layout.addWidget(self._sub)

    def _apply_accent(self, color: str) -> None:
        """Channel identity lives in a top accent bar + the label colour, so
        the value itself stays free to signal alarms."""
        self.setStyleSheet(
            f"QFrame#Readout {{ background: {theme.BG_ELEV};"
            f" border: 1px solid {theme.BORDER}; border-top: 3px solid {color};"
            " border-radius: 10px; }"
        )
        self._label.setStyleSheet(
            f"color: {color}; font-size: 10px; font-weight: 600; letter-spacing: 1.2px;"
        )

    def restyle(self, color: str) -> None:
        """Re-apply theme colours after a palette swap (new channel colour)."""
        self._accent = color
        self._apply_accent(color)
        self.set_value(self._last_value, self._last_sub)

    def set_warn_high(self, limit: float, ratio: float = 0.9) -> None:
        """Turn the value amber/red as it approaches *limit*."""
        self._warn_high = limit
        self._warn_ratio = ratio

    def set_value(self, value: float | None, sub: str = "") -> None:
        self._last_value = value
        self._last_sub = sub
        if value is None:
            self._value.setText("—")
            self._value.setStyleSheet(f"color: {theme.FG_DIM};")
        else:
            self._value.setText(f"{value:.{self._decimals}f}")
            color = theme.FG
            if self._warn_high is not None:
                if value >= self._warn_high:
                    color = theme.DANGER
                elif value >= self._warn_high * self._warn_ratio:
                    color = theme.WARN
            self._value.setStyleSheet(f"color: {color};")
        self._sub.setText(sub)


# ---------------------------------------------------------------------------
# Status banner
# ---------------------------------------------------------------------------


class StatusBanner(QLabel):
    """Coloured one-line state banner (IDLE / RUNNING / FAULT)."""

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("Banner")
        self.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        self.setMinimumHeight(32)
        # The fault detail line is long; wrap rather than truncate, since a
        # cut-off "输出已…" would hide which protection tripped.
        self.setWordWrap(True)
        self._set_style(theme.IDLE, theme.BANNER_BG, "OFFLINE")

    def _set_style(self, fg: str, bg: str, text: str) -> None:
        self.setStyleSheet(
            f"background: {bg}; color: {fg}; border: 1px solid {fg}44;"
            "border-radius: 6px; padding: 7px 12px; font-weight: 600;"
        )
        self.setText(text)

    def update_from(self, snap: LoaderSnapshot | None) -> None:
        if snap is None or snap.state is None:
            self._set_style(theme.FG_DIM, theme.BG_INPUT, "等待数据…")
            return

        name = config.STATE_NAMES.get(snap.state, "?")
        mode = config.MODE_NAMES.get(snap.mode, "—") if snap.mode is not None else "—"
        color = theme.STATE_COLOR.get(name, theme.FG_MUTED)

        if snap.is_faulted and snap.error is not None:
            err = config.ERROR_NAMES.get(snap.error, "?")
            detail = {
                "OCP": "过流保护已触发 — 输出已关断",
                "OTP": "过温保护已触发 — 输出已关断",
                "UVP": "欠压",
            }.get(err, "故障")
            self._set_style(theme.DANGER, theme.BANNER_ERR_BG, f"● ERROR · {err} · {detail}")
            return

        text = f"● {name}"
        if name == "RUNNING":
            text += f" · {mode} 模式"
        elif name == "IDLE":
            text += " · 输出关闭"
        self._set_style(color, theme.BG_INPUT, text)


# ---------------------------------------------------------------------------
# Chart
# ---------------------------------------------------------------------------


class TrendChart(QWidget):
    """Rolling multi-channel plot with per-channel visibility toggles."""

    CHANNELS = ("voltage", "current", "power", "resistance", "temperature")

    def __init__(self, window_s: float = 60.0, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.window_s = window_s
        self._t: deque[float] = deque()
        self._series: dict[str, deque[float]] = {ch: deque() for ch in self.CHANNELS}
        self._t0: float | None = None
        self._visible = {ch: ch in ("voltage", "current") for ch in self.CHANNELS}
        self._last_t: float | None = None

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(6)

        # -- toolbar
        controls = QHBoxLayout()
        controls.setSpacing(12)
        self._checks: dict[str, QCheckBox] = {}
        for ch in self.CHANNELS:
            box = QCheckBox(theme.CH_LABEL[ch])
            box.setChecked(self._visible[ch])
            box.setStyleSheet(f"color: {theme.CH_COLOR[ch]}; font-size: 11px;")
            box.toggled.connect(lambda on, c=ch: self._toggle(c, on))
            self._checks[ch] = box
            controls.addWidget(box)
        controls.addStretch(1)

        self._cursor_label = QLabel("")
        self._cursor_label.setObjectName("Value")
        controls.addWidget(self._cursor_label)
        layout.addLayout(controls)

        # -- plot
        self._plot = pg.PlotWidget()
        self._plot.setLabel("bottom", "时间", units="s")
        self._plot.setMouseEnabled(x=True, y=True)
        self._plot.setMenuEnabled(False)

        self._curves: dict[str, pg.PlotDataItem] = {}
        for ch in self.CHANNELS:
            curve = self._plot.plot(
                pen=pg.mkPen(theme.CH_COLOR[ch], width=2),
                name=theme.CH_LABEL[ch],
            )
            curve.setVisible(self._visible[ch])
            self._curves[ch] = curve

        # Cursor readout on hover.
        self._vline = pg.InfiniteLine(
            angle=90, movable=False, pen=pg.mkPen(theme.FG_DIM, style=Qt.DashLine)
        )
        self._vline.setVisible(False)
        self._plot.addItem(self._vline, ignoreBounds=True)
        self._plot.scene().sigMouseMoved.connect(self._on_mouse_move)

        self.restyle()
        layout.addWidget(self._plot, 1)

    def restyle(self) -> None:
        """Re-apply theme colours after a palette swap."""
        self._plot.setBackground(theme.BG_INPUT)
        self._plot.showGrid(x=True, y=True, alpha=theme.GRID_ALPHA)
        for axis in ("bottom", "left"):
            self._plot.getAxis(axis).setPen(theme.BORDER)
            self._plot.getAxis(axis).setTextPen(theme.FG_MUTED)
        for ch in self.CHANNELS:
            self._curves[ch].setPen(pg.mkPen(theme.CH_COLOR[ch], width=2))
            self._checks[ch].setStyleSheet(
                f"color: {theme.CH_COLOR[ch]}; font-size: 11px;"
            )
        self._vline.setPen(pg.mkPen(theme.FG_DIM, style=Qt.DashLine))

    def _toggle(self, channel: str, on: bool) -> None:
        self._visible[channel] = on
        self._curves[channel].setVisible(on)

    def _on_mouse_move(self, pos) -> None:
        if not self._t:
            return
        if not self._plot.sceneBoundingRect().contains(pos):
            self._vline.setVisible(False)
            self._cursor_label.setText("")
            return
        mouse = self._plot.getPlotItem().vb.mapSceneToView(pos)
        x = mouse.x()
        self._vline.setPos(x)
        self._vline.setVisible(True)

        # Nearest sample.
        idx = min(range(len(self._t)), key=lambda i: abs(self._t[i] - x))
        parts = [f"t={self._t[idx]:.1f}s"]
        for ch in self.CHANNELS:
            if not self._visible[ch]:
                continue
            values = self._series[ch]
            if idx < len(values):
                decimals = 1 if ch == "temperature" else 3
                parts.append(f"{ch[:3]}={values[idx]:.{decimals}f}")
        self._cursor_label.setText("  ".join(parts))

    def add_snapshot(self, snap: LoaderSnapshot, t: float) -> None:
        if not snap.has_any_measurement():
            return
        if self._t0 is None:
            self._t0 = t
        rel = t - self._t0
        # Guard against duplicate timestamps (two replies in one loop pass).
        if self._last_t is not None and rel <= self._last_t:
            rel = self._last_t
        self._last_t = rel

        self._t.append(rel)
        values = {
            "voltage": snap.voltage_measurement,
            "current": snap.current_measurement,
            "power": snap.power_measurement,
            "resistance": snap.resistance_measurement,
            "temperature": snap.temperature_measurement,
        }
        for ch, value in values.items():
            self._series[ch].append(float("nan") if value is None else value)

        # Trim to the rolling window.
        cutoff = rel - self.window_s
        while self._t and self._t[0] < cutoff:
            self._t.popleft()
            for series in self._series.values():
                series.popleft()

        x = list(self._t)
        for ch in self.CHANNELS:
            self._curves[ch].setData(x, list(self._series[ch]))

    def clear(self) -> None:
        self._t.clear()
        self._t0 = None
        self._last_t = None
        for series in self._series.values():
            series.clear()
        for curve in self._curves.values():
            curve.setData([], [])

    def set_replay(self, t: list[float], channels: dict[str, list[float | None]]) -> None:
        """Show a loaded CSV instead of live data."""
        self.clear()
        self._t0 = 0.0
        for i, ts in enumerate(t):
            self._t.append(ts)
            for ch in self.CHANNELS:
                column = channels.get(ch, [])
                value = column[i] if i < len(column) else None
                self._series[ch].append(float("nan") if value is None else value)
        x = list(self._t)
        for ch in self.CHANNELS:
            self._curves[ch].setData(x, list(self._series[ch]))
        if t:
            self._plot.setXRange(t[0], t[-1], padding=0.02)


# ---------------------------------------------------------------------------
# Terminal
# ---------------------------------------------------------------------------


class TerminalPanel(QWidget):
    """Raw shell console: shows device text and sends lines.

    Under the magic-demux design the shell is always live, so this panel doubles
    as the escape hatch when the binary path misbehaves.
    """

    line_entered = Signal(str)

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._max_blocks = 5000

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(6)

        self._view = QPlainTextEdit()
        self._view.setReadOnly(True)
        self._view.setMaximumBlockCount(self._max_blocks)
        self._view.setFont(QFont("Consolas", 10))
        self._view.setPlaceholderText("设备文本输出（回显 / 回复 / 提示符）")
        layout.addWidget(self._view, 1)

        from PySide6.QtWidgets import QLineEdit

        self._input = QLineEdit()
        self._input.setPlaceholderText("输入 shell 命令后回车（例如 lstatus）")
        self._input.setFont(QFont("Consolas", 10))
        self._input.returnPressed.connect(self._submit)
        layout.addWidget(self._input)

    def _submit(self) -> None:
        text = self._input.text().strip()
        if not text:
            return
        self._input.clear()
        self.line_entered.emit(text)

    def append(self, text: str) -> None:
        self._view.appendPlainText(text)
        self._view.moveCursor(QTextCursor.End)

    def clear(self) -> None:
        self._view.clear()


# ---------------------------------------------------------------------------
# Small labelled value row
# ---------------------------------------------------------------------------


class ValueRow(QWidget):
    """A `label : value` row used in the status grid."""

    def __init__(self, label: str, value: str = "—", parent: QWidget | None = None) -> None:
        super().__init__(parent)
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 1, 0, 1)
        layout.setSpacing(8)

        self._label = QLabel(label)
        self._label.setObjectName("FieldLabel")
        self._label.setMinimumWidth(74)

        self._value = QLabel(value)
        self._value.setObjectName("Value")
        self._value.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self._value.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)

        layout.addWidget(self._label)
        layout.addWidget(self._value, 1)

    def set_value(self, text: str, color: str | None = None) -> None:
        self._value.setText(text)
        self._value.setStyleSheet(f"color: {color};" if color else "")


__all__ = ["Card", "ReadoutCard", "StatusBanner", "TerminalPanel", "TrendChart", "ValueRow"]
