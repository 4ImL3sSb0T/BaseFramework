"""Instrument theme: colour tokens, QSS, and pyqtgraph defaults.

Two palettes ship in one module — Catppuccin Mocha (dark) and Catppuccin
Latte (light). `set_theme()` swaps the module-level tokens in place; every
consumer reads them as ``theme.X`` at call time, so a palette swap plus a
restyle pass re-skins the whole window without a restart.

Curves keep the same channel semantics in both palettes (voltage yellow,
current sapphire, power mauve, resistance green, temperature red), tuned per
palette so contrast stays readable on the respective base colour.

Kept dependency-free (no qt-material / qdarkstyle) so the look is fully under
our control and PySide6 + pyqtgraph remain the only GUI dependencies.
"""

from __future__ import annotations

# ---------------------------------------------------------------------------
# Shared constants (theme-independent)
# ---------------------------------------------------------------------------

CH_LABEL = {
    "voltage": "Voltage (V)",
    "current": "Current (A)",
    "power": "Power (W)",
    "resistance": "Resistance (Ω)",
    "temperature": "Temperature (°C)",
}

MONO_FAMILY = "'JetBrains Mono', 'Cascadia Mono', 'Consolas', 'DejaVu Sans Mono', monospace"
UI_FAMILY = "'Segoe UI', 'Inter', 'Noto Sans SC', 'Microsoft YaHei UI', sans-serif"


# ---------------------------------------------------------------------------
# Palettes
# ---------------------------------------------------------------------------
# Every key in a palette becomes a module-level token after set_theme().
# QSS-only tokens (hover / disabled / scrollbar shades) live here too so the
# stylesheet contains no hardcoded colours.

_DARK = {
    # Catppuccin Mocha
    "BG": "#181825",            # mantle
    "BG_PANEL": "#1e1e2e",      # base
    "BG_ELEV": "#2a2a3e",       # between surface0 / surface1
    "BG_INPUT": "#11111b",      # crust
    "BORDER": "#313244",        # surface0
    "BORDER_FOCUS": "#89b4fa",  # blue
    "FG": "#cdd6f4",            # text
    "FG_MUTED": "#a6adc8",      # subtext0
    "FG_DIM": "#6c7086",        # overlay0
    "ACCENT": "#89b4fa",        # blue
    "ACCENT_TEXT": "#11111b",   # text on bright accent fills (crust)
    "OK": "#a6e3a1",            # green
    "WARN": "#f9e2af",          # yellow
    "DANGER": "#f38ba8",        # red
    "IDLE": "#6c7086",          # overlay0
    "BANNER_BG": "#11111b",
    "BANNER_ERR_BG": "#36222c",
    "CH_COLOR": {
        "voltage": "#f9e2af",      # yellow
        "current": "#74c7ec",      # sapphire
        "power": "#cba6f7",        # mauve
        "resistance": "#a6e3a1",   # green
        "temperature": "#f38ba8",  # red
    },
    # QSS interaction shades
    "BTN_HOVER_BG": "#343450",
    "BTN_HOVER_BORDER": "#45456a",
    "BTN_PRESSED_BG": "#1a1a28",
    "BTN_DISABLED_BG": "#20202f",
    "BTN_DISABLED_BORDER": "#2b2b40",
    "PRIMARY_HOVER": "#9ec2fb",
    "PRIMARY_DISABLED_BG": "#2a3350",
    "DANGER_HOVER": "#f6a4ba",
    "DANGER_DISABLED_BG": "#472631",
    "SUCCESS_HOVER": "#b8ecb4",
    "SUCCESS_DISABLED_BG": "#263a28",
    "INPUT_HOVER_BORDER": "#45456a",
    "SCROLL_HANDLE": "#3b3b58",
    "SCROLL_HANDLE_HOVER": "#4c4c70",
    "GRID_ALPHA": 0.12,
}

_LIGHT = {
    # Catppuccin Latte: window base stays light-grey, cards go pure white so
    # the elevation hierarchy reads the same way as the dark theme.
    "BG": "#e6e9ef",            # mantle
    "BG_PANEL": "#ffffff",      # cards
    "BG_ELEV": "#eff1f5",       # base — readouts, buttons, chip fills
    "BG_INPUT": "#f4f6fa",      # slightly shaded inputs on white cards
    "BORDER": "#ccd0da",        # surface0
    "BORDER_FOCUS": "#1e66f5",  # blue
    "FG": "#4c4f69",            # text
    "FG_MUTED": "#6c6f85",      # subtext0
    "FG_DIM": "#9ca0b0",        # overlay0
    "ACCENT": "#1e66f5",        # blue
    "ACCENT_TEXT": "#ffffff",   # text on bright accent fills
    "OK": "#40a02b",            # green
    "WARN": "#df8e1d",          # yellow
    "DANGER": "#d20f39",        # red
    "IDLE": "#9ca0b0",          # overlay0
    "BANNER_BG": "#eff1f5",
    "BANNER_ERR_BG": "#fbe3e8",
    "CH_COLOR": {
        "voltage": "#df8e1d",      # yellow
        "current": "#209fb5",      # sapphire
        "power": "#8839ef",        # mauve
        "resistance": "#40a02b",   # green
        "temperature": "#d20f39",  # red
    },
    # QSS interaction shades
    "BTN_HOVER_BG": "#e2e6ee",
    "BTN_HOVER_BORDER": "#b8bfcc",
    "BTN_PRESSED_BG": "#d8dde7",
    "BTN_DISABLED_BG": "#eef0f4",
    "BTN_DISABLED_BORDER": "#e0e4ec",
    "PRIMARY_HOVER": "#4a86f7",
    "PRIMARY_DISABLED_BG": "#d5e1fa",
    "DANGER_HOVER": "#e1476a",
    "DANGER_DISABLED_BG": "#f3d3da",
    "SUCCESS_HOVER": "#5cb848",
    "SUCCESS_DISABLED_BG": "#d8e9d3",
    "INPUT_HOVER_BORDER": "#b8bfcc",
    "SCROLL_HANDLE": "#c3c9d6",
    "SCROLL_HANDLE_HOVER": "#a6adc0",
    "GRID_ALPHA": 0.28,
}

_PALETTES = {"dark": _DARK, "light": _LIGHT}

_current = "dark"


def _build(palette: dict) -> dict:
    """Expand a palette into the full token set (derived entries included)."""
    tokens = dict(palette)
    tokens["STATE_COLOR"] = {
        "IDLE": palette["IDLE"],
        "RUNNING": palette["OK"],
        "PAUSED": palette["WARN"],
        "ERROR": palette["DANGER"],
    }
    return tokens


def set_theme(name: str) -> None:
    """Swap the active palette. All module tokens update in place."""
    global _current
    if name not in _PALETTES:
        raise ValueError(f"unknown theme {name!r}; expected one of {sorted(_PALETTES)}")
    _current = name
    globals().update(_build(_PALETTES[name]))


def current_theme() -> str:
    return _current


def theme_names() -> list[str]:
    return sorted(_PALETTES)


# Install the default (dark) palette so importing this module always yields a
# complete token set; app startup calls set_theme() with the saved choice.
set_theme(_current)


# ---------------------------------------------------------------------------
# QSS
# ---------------------------------------------------------------------------


def stylesheet() -> str:
    return f"""
    /* The universal selector matches QLabel too, and QLabel derives from
       QFrame -- which *does* paint a styled background.  Without the explicit
       opt-out just below, every label paints its own opaque rectangle of the
       window base colour on top of whatever card it sits in: a grey band on
       white cards, a near-black band on dark cards.  Both rules have the same
       specificity, so the later one wins. */
    QWidget {{
        background: {BG};
        color: {FG};
        font-family: {UI_FAMILY};
        font-size: 13px;
    }}
    QLabel {{ background: transparent; }}

    QMainWindow, QDialog {{ background: {BG}; }}

    /* ---- Panels ---- */
    QGroupBox {{
        background: {BG_PANEL};
        border: 1px solid {BORDER};
        border-radius: 10px;
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
        color: {ACCENT};
        background: {BG};
        font-size: 11px;
        letter-spacing: 1px;
        text-transform: uppercase;
    }}

    QFrame#Card {{
        background: {BG_PANEL};
        border: 1px solid {BORDER};
        border-radius: 10px;
    }}
    QFrame#Readout {{
        background: {BG_ELEV};
        border: 1px solid {BORDER};
        border-radius: 10px;
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
        border-radius: 7px;
        padding: 6px 14px;
        color: {FG};
        min-height: 18px;
    }}
    QPushButton:hover {{ background: {BTN_HOVER_BG}; border-color: {BTN_HOVER_BORDER}; }}
    QPushButton:pressed {{ background: {BTN_PRESSED_BG}; }}
    QPushButton:disabled {{
        color: {FG_DIM}; background: {BTN_DISABLED_BG}; border-color: {BTN_DISABLED_BORDER};
    }}

    QPushButton#Primary {{
        background: {ACCENT}; border-color: {ACCENT}; color: {ACCENT_TEXT}; font-weight: 600;
    }}
    QPushButton#Primary:hover {{ background: {PRIMARY_HOVER}; border-color: {PRIMARY_HOVER}; }}
    QPushButton#Primary:disabled {{
        background: {PRIMARY_DISABLED_BG}; border-color: {PRIMARY_DISABLED_BG}; color: {FG_DIM};
    }}

    QPushButton#Danger {{
        background: {DANGER}; border-color: {DANGER}; color: {ACCENT_TEXT}; font-weight: 600;
    }}
    QPushButton#Danger:hover {{ background: {DANGER_HOVER}; border-color: {DANGER_HOVER}; }}
    QPushButton#Danger:disabled {{
        background: {DANGER_DISABLED_BG}; border-color: {DANGER_DISABLED_BG}; color: {FG_DIM};
    }}

    QPushButton#Success {{
        background: {OK}; border-color: {OK}; color: {ACCENT_TEXT}; font-weight: 600;
    }}
    QPushButton#Success:hover {{ background: {SUCCESS_HOVER}; border-color: {SUCCESS_HOVER}; }}
    QPushButton#Success:disabled {{
        background: {SUCCESS_DISABLED_BG}; border-color: {SUCCESS_DISABLED_BG}; color: {FG_DIM};
    }}

    /* Mode buttons act as a segmented control. */
    QPushButton#Mode {{ min-width: 46px; padding: 6px 4px; font-weight: 600; }}
    QPushButton#Mode:checked {{
        background: {ACCENT}; border-color: {ACCENT}; color: {ACCENT_TEXT};
    }}

    /* ---- Inputs ---- */
    QComboBox, QDoubleSpinBox, QSpinBox, QLineEdit, QPlainTextEdit {{
        background: {BG_INPUT};
        border: 1px solid {BORDER};
        border-radius: 7px;
        padding: 4px 6px;
        color: {FG};
        selection-background-color: {ACCENT};
        selection-color: {ACCENT_TEXT};
    }}
    QComboBox:hover, QDoubleSpinBox:hover, QSpinBox:hover, QLineEdit:hover {{
        border-color: {INPUT_HOVER_BORDER};
    }}
    QComboBox:focus, QDoubleSpinBox:focus, QSpinBox:focus, QLineEdit:focus {{
        border-color: {BORDER_FOCUS};
    }}
    QComboBox::drop-down {{ border: none; width: 20px; }}
    QComboBox QAbstractItemView {{
        background: {BG_PANEL};
        border: 1px solid {BORDER};
        selection-background-color: {ACCENT};
        selection-color: {ACCENT_TEXT};
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
        border-radius: 10px;
        background: {BG_PANEL};
        top: -1px;
    }}
    QTabBar::tab {{
        background: transparent;
        color: {FG_MUTED};
        padding: 7px 16px;
        border: 1px solid transparent;
        border-top-left-radius: 7px;
        border-top-right-radius: 7px;
    }}
    QTabBar::tab:selected {{
        color: {FG};
        background: {BG_PANEL};
        border-color: {BORDER};
        border-bottom-color: {BG_PANEL};
    }}
    QTabBar::tab:hover:!selected {{ color: {FG}; }}

    /* ---- Labels ---- */
    QLabel#CardTitle {{
        color: {FG_MUTED}; font-size: 11px; font-weight: 700;
        letter-spacing: 1.6px;
    }}
    QLabel#ReadoutValue {{
        font-family: {MONO_FAMILY};
        font-size: 28px;
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
        border-radius: 7px; padding: 7px 12px; font-weight: 600; font-size: 13px;
    }}

    /* ---- Scrollbars ---- */
    QScrollBar:vertical {{ background: transparent; width: 10px; margin: 0; }}
    QScrollBar::handle:vertical {{
        background: {SCROLL_HANDLE}; border-radius: 5px; min-height: 24px;
    }}
    QScrollBar::handle:vertical:hover {{ background: {SCROLL_HANDLE_HOVER}; }}
    QScrollBar:horizontal {{ background: transparent; height: 10px; }}
    QScrollBar::handle:horizontal {{
        background: {SCROLL_HANDLE}; border-radius: 5px; min-width: 24px;
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


def apply(app, name: str) -> None:
    """One-call retheme: swap palette, restyle Qt, refresh pyqtgraph defaults."""
    set_theme(name)
    configure_pyqtgraph()
    apply_to_app(app)


__all__ = [
    "ACCENT",
    "ACCENT_TEXT",
    "BANNER_BG",
    "BANNER_ERR_BG",
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
    "GRID_ALPHA",
    "IDLE",
    "OK",
    "STATE_COLOR",
    "WARN",
    "apply",
    "apply_to_app",
    "configure_pyqtgraph",
    "current_theme",
    "set_theme",
    "stylesheet",
    "theme_names",
]
