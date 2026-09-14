"""Dark instrument theme: colour tokens, QSS, and pyqtgraph defaults.

Kept dependency-free (no qt-material / qdarkstyle) so the look is fully under
our control and PySide6 + pyqtgraph remain the only GUI dependencies.

Colour choice is deliberate: a near-black neutral background with a slightly
lifted panel tone, low-saturation text, and a small set of accent colours that
match the semantics used on the curves (voltage amber, current cyan, power
violet, resistance green, temperature red).
"""

from __future__ import annotations

# ---------------------------------------------------------------------------
# Tokens
# ---------------------------------------------------------------------------

BG = "#0d1117"
BG_PANEL = "#151b23"
BG_ELEV = "#1c232d"
BG_INPUT = "#0b0f14"
BORDER = "#2a3441"
BORDER_FOCUS = "#3d7eff"

FG = "#d7dee8"
FG_MUTED = "#8b98a8"
FG_DIM = "#5c6875"

ACCENT = "#3d7eff"

OK = "#2ecc8f"
WARN = "#f2b33d"
DANGER = "#f2564b"
IDLE = "#66727f"

# Per-channel curve colours.
CH_COLOR = {
    "voltage": "#f2b33d",
    "current": "#38bdf8",
    "power": "#a78bfa",
    "resistance": "#2ecc8f",
    "temperature": "#f2564b",
}

CH_LABEL = {
    "voltage": "Voltage (V)",
    "current": "Current (A)",
    "power": "Power (W)",
    "resistance": "Resistance (Ω)",
    "temperature": "Temperature (°C)",
}

# State -> status colour.
STATE_COLOR = {
    "IDLE": IDLE,
    "RUNNING": OK,
    "PAUSED": WARN,
    "ERROR": DANGER,
}

MONO_FAMILY = "'JetBrains Mono', 'Cascadia Mono', 'Consolas', 'DejaVu Sans Mono', monospace"
UI_FAMILY = "'Segoe UI', 'Inter', 'Noto Sans SC', 'Microsoft YaHei UI', sans-serif"


# ---------------------------------------------------------------------------
# QSS
# ---------------------------------------------------------------------------


def stylesheet() -> str:
    return f"""
    QWidget {{
        background: {BG};
        color: {FG};
        font-family: {UI_FAMILY};
        font-size: 13px;
    }}

    QMainWindow, QDialog {{ background: {BG}; }}

    /* ---- Panels ---- */
    QGroupBox {{
        background: {BG_PANEL};
        border: 1px solid {BORDER};
        border-radius: 8px;
        margin-top: 14px;
        padding: 12px 10px 10px 10px;
        font-weight: 600;
    }}
    QGroupBox::title {{
        subcontrol-origin: margin;
        subcontrol-position: top left;
        left: 12px;
        top: 0px;
        padding: 1px 6px;
        color: {FG_MUTED};
        background: {BG};
        font-size: 11px;
        letter-spacing: 1px;
        text-transform: uppercase;
    }}

    QFrame#Card {{
        background: {BG_PANEL};
        border: 1px solid {BORDER};
        border-radius: 8px;
    }}
    QFrame#Readout {{
        background: {BG_ELEV};
        border: 1px solid {BORDER};
        border-radius: 8px;
    }}
    QFrame#Divider {{ background: {BORDER}; max-height: 1px; border: none; }}

    /* ---- Toolbar ---- */
    QFrame#Toolbar {{
        background: {BG_PANEL};
        border-bottom: 1px solid {BORDER};
    }}

    /* ---- Buttons ---- */
    QPushButton {{
        background: {BG_ELEV};
        border: 1px solid {BORDER};
        border-radius: 6px;
        padding: 6px 14px;
        color: {FG};
        min-height: 18px;
    }}
    QPushButton:hover {{ background: #232c37; border-color: #3a4655; }}
    QPushButton:pressed {{ background: #0f151d; }}
    QPushButton:disabled {{ color: {FG_DIM}; background: #141a21; border-color: #232c37; }}

    QPushButton#Primary {{
        background: {ACCENT}; border-color: {ACCENT}; color: #ffffff; font-weight: 600;
    }}
    QPushButton#Primary:hover {{ background: #4d8aff; border-color: #4d8aff; }}
    QPushButton#Primary:disabled {{ background: #22304a; border-color: #22304a; color: {FG_DIM}; }}

    QPushButton#Danger {{
        background: {DANGER}; border-color: {DANGER}; color: #ffffff; font-weight: 600;
    }}
    QPushButton#Danger:hover {{ background: #ff6a5f; border-color: #ff6a5f; }}
    QPushButton#Danger:disabled {{ background: #43201e; border-color: #43201e; color: {FG_DIM}; }}

    QPushButton#Success {{
        background: {OK}; border-color: {OK}; color: #06231a; font-weight: 600;
    }}
    QPushButton#Success:hover {{ background: #3fd9a0; border-color: #3fd9a0; }}
    QPushButton#Success:disabled {{ background: #17342a; border-color: #17342a; color: {FG_DIM}; }}

    /* Mode buttons act as a segmented control. */
    QPushButton#Mode {{ min-width: 46px; padding: 6px 4px; font-weight: 600; }}
    QPushButton#Mode:checked {{
        background: {ACCENT}; border-color: {ACCENT}; color: #ffffff;
    }}

    /* ---- Inputs ---- */
    QComboBox, QDoubleSpinBox, QSpinBox, QLineEdit, QPlainTextEdit {{
        background: {BG_INPUT};
        border: 1px solid {BORDER};
        border-radius: 6px;
        padding: 4px 6px;
        color: {FG};
        selection-background-color: {ACCENT};
    }}
    QComboBox:hover, QDoubleSpinBox:hover, QSpinBox:hover, QLineEdit:hover {{
        border-color: #3a4655;
    }}
    QComboBox:focus, QDoubleSpinBox:focus, QSpinBox:focus, QLineEdit:focus {{
        border-color: {BORDER_FOCUS};
    }}
    QComboBox::drop-down {{ border: none; width: 20px; }}
    QComboBox QAbstractItemView {{
        background: {BG_ELEV};
        border: 1px solid {BORDER};
        selection-background-color: {ACCENT};
        color: {FG};
        outline: none;
    }}
    QDoubleSpinBox::up-button, QDoubleSpinBox::down-button,
    QSpinBox::up-button, QSpinBox::down-button {{
        background: {BG_ELEV}; border: 1px solid {BORDER}; width: 16px;
    }}

    /* ---- Checkboxes ---- */
    QCheckBox {{ spacing: 6px; }}
    QCheckBox::indicator {{
        width: 15px; height: 15px;
        border: 1px solid {BORDER};
        border-radius: 4px;
        background: {BG_INPUT};
    }}
    QCheckBox::indicator:checked {{ background: {ACCENT}; border-color: {ACCENT}; }}
    QCheckBox::indicator:hover {{ border-color: {BORDER_FOCUS}; }}

    /* ---- Tabs ---- */
    QTabWidget::pane {{
        border: 1px solid {BORDER};
        border-radius: 8px;
        background: {BG_PANEL};
        top: -1px;
    }}
    QTabBar::tab {{
        background: transparent;
        color: {FG_MUTED};
        padding: 7px 16px;
        border: 1px solid transparent;
        border-top-left-radius: 6px;
        border-top-right-radius: 6px;
    }}
    QTabBar::tab:selected {{
        color: {FG};
        background: {BG_PANEL};
        border-color: {BORDER};
        border-bottom-color: {BG_PANEL};
    }}
    QTabBar::tab:hover:!selected {{ color: {FG}; }}

    /* ---- Labels ---- */
    QLabel#ReadoutValue {{
        font-family: {MONO_FAMILY};
        font-size: 26px;
        font-weight: 600;
        color: {FG};
    }}
    QLabel#ReadoutUnit {{ color: {FG_MUTED}; font-size: 12px; }}
    QLabel#ReadoutLabel {{
        color: {FG_MUTED}; font-size: 10px; letter-spacing: 1.2px;
        text-transform: uppercase;
    }}
    QLabel#SectionLabel {{ color: {FG_MUTED}; font-size: 11px; letter-spacing: 1px; }}
    QLabel#Hint {{ color: {FG_DIM}; font-size: 11px; }}
    QLabel#FieldLabel {{ color: {FG_MUTED}; font-size: 12px; }}
    QLabel#Value {{ font-family: {MONO_FAMILY}; font-size: 12px; }}
    QLabel#BigValue {{ font-family: {MONO_FAMILY}; font-size: 15px; font-weight: 600; }}

    /* ---- Status banner ---- */
    QLabel#Banner {{
        border-radius: 6px; padding: 7px 12px; font-weight: 600; font-size: 13px;
    }}

    /* ---- Scrollbars ---- */
    QScrollBar:vertical {{ background: transparent; width: 10px; margin: 0; }}
    QScrollBar::handle:vertical {{
        background: #2f3a47; border-radius: 5px; min-height: 24px;
    }}
    QScrollBar::handle:vertical:hover {{ background: #3d4a5a; }}
    QScrollBar:horizontal {{ background: transparent; height: 10px; }}
    QScrollBar::handle:horizontal {{
        background: #2f3a47; border-radius: 5px; min-width: 24px;
    }}
    QScrollBar::add-line, QScrollBar::sub-line {{ height: 0; width: 0; }}
    QScrollBar::add-page, QScrollBar::sub-page {{ background: transparent; }}

    /* ---- Splitter ---- */
    QSplitter::handle {{ background: {BORDER}; }}
    QSplitter::handle:horizontal {{ width: 1px; }}
    QSplitter::handle:vertical {{ height: 1px; }}

    /* ---- Status bar ---- */
    QStatusBar {{ background: {BG_PANEL}; border-top: 1px solid {BORDER}; color: {FG_MUTED}; }}
    QStatusBar::item {{ border: none; }}
    """


def apply_to_app(app) -> None:
    app.setStyleSheet(stylesheet())


def configure_pyqtgraph() -> None:
    """Apply theme colours to pyqtgraph's global config."""
    import pyqtgraph as pg

    pg.setConfigOption("background", BG_PANEL)
    pg.setConfigOption("foreground", FG_MUTED)
    pg.setConfigOptions(antialias=True)


__all__ = [
    "BG",
    "BG_ELEV",
    "BG_INPUT",
    "BG_PANEL",
    "BORDER",
    "CH_COLOR",
    "CH_LABEL",
    "DANGER",
    "FG",
    "FG_DIM",
    "FG_MUTED",
    "IDLE",
    "OK",
    "STATE_COLOR",
    "WARN",
    "apply_to_app",
    "configure_pyqtgraph",
    "stylesheet",
]
