"""Main window: layout, wiring and user actions.

Layout (left: control, centre: charts, right: status), with a tabbed bottom
area for the raw terminal and the binary link diagnostics.

All device interaction goes through `HostClient`, so this file contains no
threading and no serial code -- only presentation and intent.
"""

from __future__ import annotations

import time
from pathlib import Path

from PySide6.QtCore import QSettings, Qt, Slot
from PySide6.QtWidgets import (
    QApplication,
    QButtonGroup,
    QCheckBox,
    QComboBox,
    QDoubleSpinBox,
    QFileDialog,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QSplitter,
    QStatusBar,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

from . import config, theme
from .client import HostClient, PollConfig, list_serial_ports
from .model import CommandResult, LoaderSnapshot
from .panels import Card, ReadoutCard, StatusBanner, TerminalPanel, TrendChart, ValueRow
from .recorder import CsvRecorder, load_csv

SIM_PORT = "__sim__"


class MainWindow(QMainWindow):
    def __init__(
        self,
        *,
        start_sim: bool = False,
        port: str | None = None,
        baudrate: int = config.DEFAULT_BAUDRATE,
    ) -> None:
        super().__init__()
        self.setWindowTitle("电子负载上位机 · STM32H750")
        self.resize(1440, 900)
        self.setMinimumSize(1100, 700)

        self._settings = QSettings("BaseFramework", "loader_host")
        self._client = HostClient(self)
        self._snap = LoaderSnapshot()
        self._recorder: CsvRecorder | None = None
        self._connected = False
        self._last_result_ok: bool | None = None
        self._link_desc = ""

        self._build_ui()
        self._wire()
        self._restore_settings()

        if start_sim:
            self._port_combo.setCurrentIndex(0)  # simulator entry
            self._on_connect_clicked()
        elif port:
            self._connect_to(port, baudrate)

    # ------------------------------------------------------------------
    # Construction
    # ------------------------------------------------------------------

    def _build_ui(self) -> None:
        root = QWidget()
        root_layout = QVBoxLayout(root)
        root_layout.setContentsMargins(0, 0, 0, 0)
        root_layout.setSpacing(0)

        root_layout.addWidget(self._build_toolbar())

        body = QWidget()
        body_layout = QHBoxLayout(body)
        body_layout.setContentsMargins(12, 12, 12, 8)
        body_layout.setSpacing(12)

        body_layout.addWidget(self._build_left_column(), 0)

        centre = QWidget()
        centre_layout = QVBoxLayout(centre)
        centre_layout.setContentsMargins(0, 0, 0, 0)
        centre_layout.setSpacing(12)
        self._banner = StatusBanner()
        centre_layout.addWidget(self._banner)
        centre_layout.addWidget(self._build_readouts())
        centre_layout.addWidget(self._build_chart(), 1)
        body_layout.addWidget(centre, 1)

        body_layout.addWidget(self._build_right_column(), 0)

        splitter = QSplitter(Qt.Vertical)
        splitter.addWidget(body)
        splitter.addWidget(self._build_bottom_tabs())
        splitter.setStretchFactor(0, 4)
        splitter.setStretchFactor(1, 1)
        splitter.setSizes([640, 220])
        root_layout.addWidget(splitter, 1)

        self.setCentralWidget(root)
        self.setStatusBar(QStatusBar())

    # -- toolbar ----------------------------------------------------------

    def _build_toolbar(self) -> QWidget:
        bar = QFrame()
        bar.setObjectName("Toolbar")
        layout = QHBoxLayout(bar)
        layout.setContentsMargins(12, 8, 12, 8)
        layout.setSpacing(8)

        self._title_label = QLabel("LOADER HOST")
        self._title_label.setObjectName("SectionLabel")
        self._apply_title_style()
        layout.addWidget(self._title_label)

        layout.addSpacing(16)

        layout.addWidget(self._muted_label("端口"))
        self._port_combo = QComboBox()
        self._port_combo.setMinimumWidth(240)
        layout.addWidget(self._port_combo)

        refresh = QPushButton("刷新")
        refresh.setToolTip("刷新串口列表")
        refresh.setMinimumWidth(52)
        refresh.clicked.connect(self._refresh_ports)
        layout.addWidget(refresh)

        layout.addWidget(self._muted_label("波特率"))
        self._baud_combo = QComboBox()
        for rate in config.BAUDRATE_CHOICES:
            self._baud_combo.addItem(str(rate), rate)
        self._baud_combo.setCurrentText(str(config.DEFAULT_BAUDRATE))
        layout.addWidget(self._baud_combo)

        self._connect_btn = QPushButton("连接")
        self._connect_btn.setObjectName("Primary")
        self._connect_btn.setMinimumWidth(88)
        self._connect_btn.clicked.connect(self._on_connect_clicked)
        layout.addWidget(self._connect_btn)

        layout.addStretch(1)

        self._theme_btn = QPushButton()
        self._theme_btn.setToolTip("切换浅色 / 深色主题")
        self._theme_btn.clicked.connect(self._on_theme_toggled)
        layout.addWidget(self._theme_btn)
        self._update_theme_btn()

        self._link_chip = QLabel()
        self._set_link_chip(False, "")
        layout.addWidget(self._link_chip)

        return bar

    def _apply_title_style(self) -> None:
        self._title_label.setStyleSheet(
            f"color: {theme.ACCENT}; font-size: 13px; font-weight: 700; letter-spacing: 2px;"
        )

    def _set_link_chip(self, connected: bool, description: str) -> None:
        """Connection status pill on the right of the toolbar."""
        self._link_desc = description
        if connected:
            dot, text = theme.OK, description
        else:
            dot, text = theme.FG_DIM, "未连接"
        self._link_chip.setText(f"●  {text}")
        self._link_chip.setStyleSheet(
            f"color: {dot}; background: {theme.BG_ELEV};"
            f" border: 1px solid {theme.BORDER}; border-radius: 12px;"
            " padding: 4px 14px; font-weight: 600; font-size: 12px;"
        )

    def _muted_label(self, text: str) -> QLabel:
        label = QLabel(text)
        label.setObjectName("FieldLabel")
        return label

    # -- readouts ---------------------------------------------------------

    def _build_readouts(self) -> QWidget:
        # Each ReadoutCard is already a styled card, so the row itself is a
        # plain transparent container.
        row_widget = QWidget()
        layout = QHBoxLayout(row_widget)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(10)

        self._read_voltage = ReadoutCard("电压", "V", theme.CH_COLOR["voltage"], 3)
        self._read_current = ReadoutCard("电流", "A", theme.CH_COLOR["current"], 3)
        self._read_power = ReadoutCard("功率", "W", theme.CH_COLOR["power"], 3)
        self._read_resistance = ReadoutCard("电阻", "Ω", theme.CH_COLOR["resistance"], 2)
        self._read_temperature = ReadoutCard("温度", "°C", theme.CH_COLOR["temperature"], 1)

        self._read_voltage.set_warn_high(config.VOLTAGE_MAX, 0.9)
        self._read_current.set_warn_high(config.CURRENT_MAX, 0.9)
        self._read_power.set_warn_high(config.POWER_MAX, 0.9)
        self._read_temperature.set_warn_high(config.OVERTEMPERATURE_LIMIT, 0.85)

        for widget in (
            self._read_voltage,
            self._read_current,
            self._read_power,
            self._read_resistance,
            self._read_temperature,
        ):
            layout.addWidget(widget, 1)

        return row_widget

    def _build_chart(self) -> QWidget:
        card = Card("实时曲线")
        card.body.setContentsMargins(12, 10, 12, 12)
        self._chart = TrendChart(window_s=60.0)
        card.body.addWidget(self._chart, 1)
        return card

    # -- left column: control ---------------------------------------------

    def _build_left_column(self) -> QWidget:
        # The control stack is taller than the window at small heights, so it
        # gets its own scroll area; without one Qt compresses the cards into
        # each other and the contents overlap.
        from PySide6.QtWidgets import QScrollArea

        inner = QWidget()
        layout = QVBoxLayout(inner)
        layout.setContentsMargins(0, 0, 6, 0)
        layout.setSpacing(12)

        layout.addWidget(self._build_control_card())
        layout.addWidget(self._build_output_card())
        layout.addWidget(self._build_fan_card())
        layout.addWidget(self._build_record_card())
        layout.addStretch(1)

        scroll = QScrollArea()
        scroll.setWidget(inner)
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        scroll.setFixedWidth(300)
        scroll.setStyleSheet("QScrollArea { background: transparent; }")
        return scroll

    def _build_control_card(self) -> QWidget:
        """Mode selector + setpoint in one card (they only make sense together)."""
        card = Card("控制")
        body = card.body

        # -- mode segmented buttons
        mode_row = QHBoxLayout()
        mode_row.setSpacing(6)
        self._mode_group = QButtonGroup(self)
        self._mode_buttons: dict[config.LoaderMode, QPushButton] = {}
        for mode in (
            config.LoaderMode.CC,
            config.LoaderMode.CV,
            config.LoaderMode.CP,
            config.LoaderMode.CR,
        ):
            button = QPushButton(config.MODE_NAMES[mode])
            button.setObjectName("Mode")
            button.setCheckable(True)
            button.clicked.connect(lambda _=False, m=mode: self._on_mode_clicked(m))
            self._mode_group.addButton(button)
            self._mode_buttons[mode] = button
            mode_row.addWidget(button)
        body.addLayout(mode_row)

        caption = QLabel("CC 恒流 · CV 恒压 · CP 恒功率 · CR 恒阻")
        caption.setObjectName("Hint")
        caption.setWordWrap(True)
        body.addWidget(caption)

        divider = QFrame()
        divider.setObjectName("Divider")
        body.addWidget(divider)

        # -- setpoint
        self._setpoint_label = QLabel("—")
        self._setpoint_label.setObjectName("FieldLabel")
        body.addWidget(self._setpoint_label)

        row = QHBoxLayout()
        row.setSpacing(6)
        self._setpoint_spin = QDoubleSpinBox()
        self._setpoint_spin.setDecimals(3)
        self._setpoint_spin.setSingleStep(0.1)
        self._setpoint_spin.setMinimum(0.0)
        self._setpoint_spin.setMaximum(config.CURRENT_MAX)
        self._setpoint_spin.setKeyboardTracking(False)
        row.addWidget(self._setpoint_spin, 1)
        self._setpoint_unit = QLabel("A")
        self._setpoint_unit.setObjectName("Value")
        self._setpoint_unit.setMinimumWidth(30)
        row.addWidget(self._setpoint_unit)
        apply_btn = QPushButton("下发")
        apply_btn.setObjectName("Primary")
        apply_btn.clicked.connect(self._on_apply_setpoint)
        row.addWidget(apply_btn)
        body.addLayout(row)

        self._limit_hint = QLabel("")
        self._limit_hint.setObjectName("Hint")
        body.addWidget(self._limit_hint)

        self._update_setpoint_widgets(config.LoaderMode.CC)
        return card

    def _build_output_card(self) -> QWidget:
        card = Card("输出")
        body = card.body

        row = QHBoxLayout()
        row.setSpacing(8)
        self._run_btn = QPushButton("启动输出")
        self._run_btn.setObjectName("Success")
        self._run_btn.clicked.connect(lambda: self._client.op("run"))
        self._stop_btn = QPushButton("停止")
        self._stop_btn.setObjectName("Danger")
        self._stop_btn.clicked.connect(lambda: self._client.op("stop"))
        row.addWidget(self._run_btn, 1)
        row.addWidget(self._stop_btn, 1)
        body.addLayout(row)

        self._clear_btn = QPushButton("清除故障 (lclr)")
        self._clear_btn.clicked.connect(lambda: self._client.op("clear_fault"))
        body.addWidget(self._clear_btn)

        hint = QLabel("启动仅在无故障时有效；故障需先清除。")
        hint.setObjectName("Hint")
        hint.setWordWrap(True)
        body.addWidget(hint)
        return card

    def _build_fan_card(self) -> QWidget:
        card = Card("风扇")
        body = card.body

        row = QHBoxLayout()
        row.setSpacing(8)
        self._fan_on_btn = QPushButton("开")
        self._fan_on_btn.clicked.connect(lambda: self._client.op("fan_on"))
        self._fan_off_btn = QPushButton("关")
        self._fan_off_btn.clicked.connect(lambda: self._client.op("fan_off"))
        row.addWidget(self._fan_on_btn, 1)
        row.addWidget(self._fan_off_btn, 1)
        body.addLayout(row)

        row2 = QHBoxLayout()
        row2.setSpacing(6)
        self._fan_slider = QDoubleSpinBox()
        self._fan_slider.setRange(0.0, 100.0)
        self._fan_slider.setDecimals(0)
        self._fan_slider.setSingleStep(5)
        self._fan_slider.setValue(50)
        self._fan_slider.setSuffix(" %")
        row2.addWidget(self._fan_slider, 1)
        fan_set = QPushButton("设定")
        fan_set.clicked.connect(lambda: self._client.op("fan_percent", self._fan_slider.value()))
        row2.addWidget(fan_set)
        body.addLayout(row2)

        self._fan_readout = ValueRow("状态", "—")
        body.addWidget(self._fan_readout)
        return card

    def _build_record_card(self) -> QWidget:
        card = Card("记录")
        body = card.body

        self._record_btn = QPushButton("开始记录 CSV")
        self._record_btn.setCheckable(True)
        self._record_btn.clicked.connect(self._on_record_toggled)
        body.addWidget(self._record_btn)

        row = QHBoxLayout()
        row.setSpacing(8)
        export_btn = QPushButton("导出…")
        export_btn.clicked.connect(self._on_export)
        replay_btn = QPushButton("回放…")
        replay_btn.clicked.connect(self._on_replay)
        row.addWidget(export_btn, 1)
        row.addWidget(replay_btn, 1)
        body.addLayout(row)

        self._record_label = QLabel("未记录")
        self._record_label.setObjectName("Hint")
        self._record_label.setWordWrap(True)
        body.addWidget(self._record_label)
        return card

    # -- right column: status ---------------------------------------------

    def _build_right_column(self) -> QWidget:
        from PySide6.QtWidgets import QScrollArea

        inner = QWidget()
        layout = QVBoxLayout(inner)
        layout.setContentsMargins(0, 0, 6, 0)
        layout.setSpacing(12)

        layout.addWidget(self._build_status_box())
        layout.addWidget(self._build_output_box())
        layout.addWidget(self._build_limits_box())
        layout.addStretch(1)

        scroll = QScrollArea()
        scroll.setWidget(inner)
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        scroll.setFixedWidth(300)
        scroll.setStyleSheet("QScrollArea { background: transparent; }")
        return scroll

    def _build_status_box(self) -> QWidget:
        card = Card("状态")
        body = card.body
        body.setSpacing(4)

        self._row_state = ValueRow("状态")
        self._row_error = ValueRow("故障")
        self._row_mode = ValueRow("模式")
        self._row_active = ValueRow("当前设定")
        body.addWidget(self._row_state)
        body.addWidget(self._row_error)
        body.addWidget(self._row_mode)
        body.addWidget(self._row_active)

        note = QLabel("电流为下发值（硬件闭环），非 ADC 采样。")
        note.setObjectName("Hint")
        note.setWordWrap(True)
        body.addWidget(note)
        return card

    def _build_output_box(self) -> QWidget:
        card = Card("执行器读回")
        body = card.body
        body.setSpacing(4)

        self._row_out_en = ValueRow("使能")
        self._row_out_i = ValueRow("Iref")
        self._row_out_v = ValueRow("Vref")
        self._row_out_norm = ValueRow("归一化")
        for row in (self._row_out_en, self._row_out_i, self._row_out_v, self._row_out_norm):
            body.addWidget(row)
        return card

    def _build_limits_box(self) -> QWidget:
        card = Card("限值")
        body = card.body
        body.setSpacing(4)

        self._row_ocp = ValueRow("OCP", f"{config.OVERCURRENT_LIMIT:.2f} A")
        self._row_otp = ValueRow("OTP", f"{config.OVERTEMPERATURE_LIMIT:.0f} °C")
        self._row_imax = ValueRow("Imax", f"{config.CURRENT_MAX:.2f} A")
        self._row_vmax = ValueRow("Vmax", f"{config.VOLTAGE_MAX:.1f} V")
        self._row_pmax = ValueRow("Pmax", f"{config.POWER_MAX:.0f} W")
        self._row_rmax = ValueRow("Rmax", f"{config.RESISTANCE_MAX:.0f} Ω")
        for row in (
            self._row_ocp,
            self._row_otp,
            self._row_imax,
            self._row_vmax,
            self._row_pmax,
            self._row_rmax,
        ):
            body.addWidget(row)
        return card

    # -- bottom tabs ------------------------------------------------------

    def _build_bottom_tabs(self) -> QWidget:
        tabs = QTabWidget()
        tabs.setDocumentMode(True)

        self._terminal = TerminalPanel()
        tabs.addTab(self._terminal, "串口终端")

        tabs.addTab(self._build_binary_panel(), "二进制协议")
        return tabs

    def _build_binary_panel(self) -> QWidget:
        panel = QWidget()
        layout = QVBoxLayout(panel)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(8)

        header = QHBoxLayout()
        self._binary_chk = QCheckBox("启用二进制链路轮询")
        self._binary_chk.toggled.connect(self._on_binary_toggled)
        header.addWidget(self._binary_chk)

        self._binary_hz = QDoubleSpinBox()
        self._binary_hz.setRange(0.0, 50.0)
        self._binary_hz.setValue(0.0)
        self._binary_hz.setDecimals(1)
        self._binary_hz.setSuffix(" Hz")
        self._binary_hz.setToolTip("0 = 关闭；由设备侧 TELEMETRY 或 GET_STATUS 回复驱动")
        self._binary_hz.valueChanged.connect(self._push_poll_config)
        header.addWidget(self._binary_hz)

        ping_btn = QPushButton("发送 PING")
        ping_btn.clicked.connect(lambda: self._client.op("ping"))
        header.addWidget(ping_btn)
        header.addStretch(1)
        layout.addLayout(header)

        warning = QLabel(
            "⚠ 固件当前尚未实现二进制协议。此链路已按 PROTOCOL.md 完整实现并有单元测试，"
            "待固件支持后即可启用。"
        )
        warning.setObjectName("Hint")
        warning.setWordWrap(True)
        layout.addWidget(warning)

        # Statistics as a compact table: label and value must stay adjacent,
        # so each cell is its own little HBox rather than a stretched row.
        grid = QGridLayout()
        grid.setHorizontalSpacing(28)
        grid.setVerticalSpacing(6)
        self._binary_stats: dict[str, QLabel] = {}
        fields = [
            ("tx_bytes", "发送字节"),
            ("rx_bytes", "接收字节"),
            ("frames_ok", "有效帧"),
            ("crc_errors", "CRC 错误"),
            ("resyncs", "重同步"),
            ("parse_hits", "解析命中"),
        ]
        for index, (key, label) in enumerate(fields):
            cell = QWidget()
            cell_layout = QHBoxLayout(cell)
            cell_layout.setContentsMargins(0, 0, 0, 0)
            cell_layout.setSpacing(8)
            name = QLabel(label)
            name.setObjectName("FieldLabel")
            value = QLabel("0")
            value.setObjectName("BigValue")
            value.setMinimumWidth(64)
            cell_layout.addWidget(name)
            cell_layout.addWidget(value)
            cell_layout.addStretch(1)
            self._binary_stats[key] = value
            grid.addWidget(cell, index // 3, index % 3)
        layout.addLayout(grid)

        layout.addStretch(1)
        return panel

    # ------------------------------------------------------------------
    # Wiring
    # ------------------------------------------------------------------

    def _wire(self) -> None:
        self._client.snapshot_updated.connect(self._on_snapshot)
        self._client.line_received.connect(self._terminal.append)
        self._client.result_received.connect(self._on_result)
        self._client.connection_changed.connect(self._on_connection_changed)
        self._client.error_occurred.connect(self._on_error)
        self._client.stats_updated.connect(self._on_stats)

        self._terminal.line_entered.connect(self._on_terminal_line)

        self._poll = PollConfig()
        self._push_poll_config()

    def _push_poll_config(self) -> None:
        config_obj = PollConfig(
            meas_hz=10.0,
            status_hz=1.0,
            binary_hz=self._binary_hz.value() if hasattr(self, "_binary_hz") else 0.0,
        )
        self._client.set_poll(config_obj)

    def _restore_settings(self) -> None:
        self._refresh_ports()
        baud = self._settings.value("baudrate", config.DEFAULT_BAUDRATE, type=int)
        index = self._baud_combo.findData(baud)
        if index >= 0:
            self._baud_combo.setCurrentIndex(index)

    def _refresh_ports(self) -> None:
        current = self._port_combo.currentData()
        self._port_combo.clear()
        self._port_combo.addItem("◈ 模拟器（无硬件）", SIM_PORT)
        for device, label in list_serial_ports():
            self._port_combo.addItem(label, device)
        if current:
            index = self._port_combo.findData(current)
            if index >= 0:
                self._port_combo.setCurrentIndex(index)

    # ------------------------------------------------------------------
    # Theme switching
    # ------------------------------------------------------------------

    def _update_theme_btn(self) -> None:
        # The button advertises the theme it will switch *to*.
        if theme.current_theme() == "light":
            self._theme_btn.setText("☾ 深色")
        else:
            self._theme_btn.setText("☀ 浅色")

    def _on_theme_toggled(self) -> None:
        name = "dark" if theme.current_theme() == "light" else "light"
        theme.apply(QApplication.instance(), name)
        self._settings.setValue("theme", name)
        self._restyle()

    def _restyle(self) -> None:
        """Refresh everything that carries inline (non-QSS) theme colours."""
        self._update_theme_btn()
        self._apply_title_style()
        self._set_link_chip(self._connected, self._link_desc)
        for card, channel in (
            (self._read_voltage, "voltage"),
            (self._read_current, "current"),
            (self._read_power, "power"),
            (self._read_resistance, "resistance"),
            (self._read_temperature, "temperature"),
        ):
            card.restyle(theme.CH_COLOR[channel])
        self._banner.update_from(self._snap if self._connected else None)
        self._chart.restyle()

    # ------------------------------------------------------------------
    # Slots
    # ------------------------------------------------------------------

    def _on_connect_clicked(self) -> None:
        if self._connected:
            self._client.disconnect_port()
            return
        port = self._port_combo.currentData()
        baud = self._baud_combo.currentData() or config.DEFAULT_BAUDRATE
        self._connect_to(port, baud)

    def _connect_to(self, port: str | None, baudrate: int) -> None:
        self._settings.setValue("baudrate", baudrate)
        self._set_link_chip(False, "连接中…")
        self._client.connect_port(port, baudrate)

    @Slot(bool, str)
    def _on_connection_changed(self, connected: bool, description: str) -> None:
        self._connected = connected
        if connected:
            self._connect_btn.setText("断开")
            self._connect_btn.setObjectName("Danger")
            self._set_link_chip(True, description)
            self._chart.clear()
            self._snap = LoaderSnapshot()
        else:
            self._connect_btn.setText("连接")
            self._connect_btn.setObjectName("Primary")
            self._set_link_chip(False, "")
            self._banner.update_from(None)
        # Re-apply QSS so the objectName change takes effect.
        self._connect_btn.style().unpolish(self._connect_btn)
        self._connect_btn.style().polish(self._connect_btn)

    @Slot(object)
    def _on_snapshot(self, snap: LoaderSnapshot) -> None:
        self._snap = snap
        self._update_readouts(snap)
        self._update_status(snap)
        self._chart.add_snapshot(snap, snap.rx_time)
        if self._recorder is not None:
            self._recorder.write(snap)
            self._record_label.setText(f"记录中：{self._recorder.row_count} 行")

    @Slot(object)
    def _on_result(self, result: CommandResult) -> None:
        if result.ok:
            self.statusBar().showMessage(f"{result.command}: OK", 4000)
        else:
            message = result.detail
            if result.exit_code == config.EXIT_BUSY:
                QMessageBox.warning(
                    self,
                    "启动被拒绝",
                    "设备处于 ERROR 状态，输出被锁定。\n请先点击「清除故障 (lclr)」。",
                )
            self.statusBar().showMessage(f"{result.command}: 失败 — {message}", 6000)

    @Slot(str)
    def _on_error(self, message: str) -> None:
        self.statusBar().showMessage(message, 6000)

    @Slot(object)
    def _on_stats(self, stats) -> None:
        for key, label in self._binary_stats.items():
            label.setText(str(getattr(stats, key, 0)))

    def _on_terminal_line(self, text: str) -> None:
        self._client.send_line(text)

    def _on_mode_clicked(self, mode: config.LoaderMode) -> None:
        self._client.op("mode", mode)
        self._update_setpoint_widgets(mode)

    def _on_apply_setpoint(self) -> None:
        value = self._setpoint_spin.value()
        self._client.op("set", value, None)
        self.statusBar().showMessage(f"已下发设定 {value:.3f}", 3000)

    def _on_binary_toggled(self, enabled: bool) -> None:
        self._client.set_binary_enabled(enabled)
        if enabled and self._binary_hz.value() == 0.0:
            self._binary_hz.setValue(2.0)

    def _on_record_toggled(self, checked: bool) -> None:
        if checked:
            path = Path("logs") / f"loader_{time.strftime('%Y%m%d_%H%M%S')}.csv"
            self._recorder = CsvRecorder(path, min_interval_s=0.05)
            self._recorder.start()
            self._record_btn.setText("停止记录")
            self._record_label.setText(f"记录中：{path}")
        else:
            if self._recorder is not None:
                rows = self._recorder.row_count
                path = self._recorder.path
                self._recorder.stop()
                self._recorder = None
                self._record_label.setText(f"已保存 {rows} 行 → {path}")
            self._record_btn.setText("开始记录 CSV")

    def _on_export(self) -> None:
        if self._recorder is not None:
            QMessageBox.information(self, "提示", "请先停止记录，再导出。")
            return
        path, _ = QFileDialog.getSaveFileName(
            self, "导出 CSV", str(Path("logs") / "loader_export.csv"), "CSV (*.csv)"
        )
        if not path:
            return
        recorder = CsvRecorder(path)
        recorder.start()
        recorder.write(self._snap, force=True)
        recorder.stop()
        self.statusBar().showMessage(f"已导出当前快照 → {path}", 5000)

    def _on_replay(self) -> None:
        path, _ = QFileDialog.getOpenFileName(self, "选择 CSV 回放", "logs", "CSV (*.csv)")
        if not path:
            return
        try:
            data = load_csv(path)
        except Exception as exc:
            QMessageBox.critical(self, "回放失败", str(exc))
            return
        self._chart.set_replay(data.t, data.channels)
        self.statusBar().showMessage(f"已载入 {len(data.t)} 行为回放 — {path}", 5000)

    # ------------------------------------------------------------------
    # Rendering helpers
    # ------------------------------------------------------------------

    def _update_readouts(self, snap: LoaderSnapshot) -> None:
        self._read_voltage.set_value(snap.voltage_measurement)
        self._read_current.set_value(snap.current_measurement, sub="下发值")
        self._read_power.set_value(snap.power_measurement)
        self._read_resistance.set_value(snap.resistance_measurement)
        self._read_temperature.set_value(snap.temperature_measurement)

    def _update_status(self, snap: LoaderSnapshot) -> None:
        if snap.state is not None:
            name = config.STATE_NAMES.get(snap.state, "?")
            self._row_state.set_value(name, theme.STATE_COLOR.get(name))
        if snap.error is not None:
            err = config.ERROR_NAMES.get(snap.error, "?")
            self._row_error.set_value(err, theme.DANGER if err != "NONE" else theme.OK)
        if snap.mode is not None:
            name = config.MODE_NAMES.get(snap.mode, "?")
            self._row_mode.set_value(name)
            self._sync_mode_buttons(snap.mode)
            self._update_setpoint_widgets(snap.mode)
        active = snap.active_setpoint
        if active is not None and snap.mode is not None:
            unit = config.MODE_META[snap.mode]["unit"]
            self._row_active.set_value(f"{active:.3f} {unit}")

        self._row_out_en.set_value(
            "ON" if snap.out_enabled else "OFF",
            theme.OK if snap.out_enabled else theme.FG_MUTED,
        )
        if snap.out_current is not None:
            self._row_out_i.set_value(f"{snap.out_current:.3f} A")
        if snap.out_voltage is not None:
            self._row_out_v.set_value(f"{snap.out_voltage:.3f} V")
        if snap.out_norm is not None:
            self._row_out_norm.set_value(f"{snap.out_norm:.4f}")

        if snap.fan_enabled is not None or snap.fan_speed is not None:
            enabled = "开" if snap.fan_enabled else "关"
            speed = f"{(snap.fan_speed or 0) * 100:.0f}%"
            self._fan_readout.set_value(f"{enabled} · {speed}")

        if snap.ocp_limit is not None:
            self._row_ocp.set_value(f"{snap.ocp_limit:.2f} A")
        if snap.otp_limit is not None:
            self._row_otp.set_value(f"{snap.otp_limit:.0f} °C")

        self._banner.update_from(snap)

    def _sync_mode_buttons(self, mode: config.LoaderMode) -> None:
        button = self._mode_buttons.get(mode)
        if button is not None and not button.isChecked():
            button.setChecked(True)

    def _update_setpoint_widgets(self, mode: config.LoaderMode) -> None:
        meta = config.MODE_META[mode]
        self._setpoint_label.setText(
            f"{meta['label_cn']}设定（最大 {meta['limit']:g} {meta['unit']}）"
        )
        self._setpoint_unit.setText(meta["unit"])
        self._limit_hint.setText(f"固件会静默钳位到 [0, {meta['limit']:g}] {meta['unit']}")
        # Widening before narrowing avoids Qt clamping the old value.
        self._setpoint_spin.setMaximum(1e9)
        self._setpoint_spin.setMaximum(meta["limit"])
        self._setpoint_spin.setMinimum(0.0)
        if self._snap.active_setpoint is not None and self._snap.mode == mode:
            self._setpoint_spin.setValue(self._snap.active_setpoint)
        else:
            self._setpoint_spin.setValue(meta["default"])

    # ------------------------------------------------------------------

    def closeEvent(self, event) -> None:
        if self._recorder is not None:
            self._recorder.stop()
        self._settings.setValue("geometry", self.saveGeometry())
        self._client.shutdown()
        super().closeEvent(event)
