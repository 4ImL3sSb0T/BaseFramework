"""Firmware constants mirrored on the host side.

Every value here is copied from the C headers so the GUI can clamp inputs and
annotate limits without asking the device. Keep in sync with:

  src/app/loader_config.h   -> limits, defaults, control period
  src/app/loader_runtime.h  -> state / error / mode enums
  src/service/fan/fan.h     -> fan duty range

If the firmware constants change, change them here too; nothing else in this
package hardcodes a limit.
"""

from __future__ import annotations

from enum import IntEnum

# ---------------------------------------------------------------------------
# Limits (loader_config.h)
# ---------------------------------------------------------------------------

CURRENT_MAX = 5.0  # LOADER_CURRENT_MAX, A
VOLTAGE_MAX = 30.0  # LOADER_VOLTAGE_MAX, V
POWER_MAX = 50.0  # LOADER_POWER_MAX, W
RESISTANCE_MAX = 1000.0  # LOADER_RESISTANCE_MAX, ohm

OVERCURRENT_LIMIT = 5.0  # LOADER_OVERCURRENT_LIMIT, A
OVERTEMPERATURE_LIMIT = 50.0  # LOADER_OVERTEMPERATURE_LIMIT, degC

# Defaults (loader_config.h)
DEFAULT_CURRENT_SETPOINT = 0.0
DEFAULT_VOLTAGE_SETPOINT = 0.0
DEFAULT_POWER_SETPOINT = 0.0
DEFAULT_RESISTANCE_SETPOINT = 1000.0

# Clamp bounds enforced by loader_runtime.c (resistance never reaches 0)
RESISTANCE_MIN = 0.01
CURRENT_MIN = 0.0
VOLTAGE_MIN = 0.0
POWER_MIN = 0.0

CONTROL_PERIOD_S = 0.0005  # LOADER_CONTROL_PERIOD_S, ~2 kHz TIM16
USE_SOFTWARE_CURRENT_PID = False  # LOADER_USE_SOFTWARE_CURRENT_PID == 0

OUT_RESOLUTION = 4095  # LOAD_OUT_RESOLUTION, 12-bit DAC
OUT_VOLTAGE_MAX = 3.3  # LOAD_OUT_VOLTAGE_MAX, V

# ---------------------------------------------------------------------------
# Serial defaults (Core/Src/usart.c)
# ---------------------------------------------------------------------------

DEFAULT_BAUDRATE = 115200
BAUDRATE_CHOICES = (9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600)

# ---------------------------------------------------------------------------
# Enums (loader_runtime.h) — the CLI string names live in loader_cli.c
# ---------------------------------------------------------------------------


class LoaderState(IntEnum):
    IDLE = 0
    RUNNING = 1
    PAUSED = 2  # reserved: no firmware code path produces this
    ERROR = 3


class LoaderError(IntEnum):
    NONE = 0
    OVERCURRENT = 1
    OVERTEMPERATURE = 2
    UNDERVOLTAGE = 3  # reserved: no firmware code path produces this


class LoaderMode(IntEnum):
    CC = 0
    CV = 1
    CP = 2
    CR = 3


STATE_NAMES = {
    LoaderState.IDLE: "IDLE",
    LoaderState.RUNNING: "RUNNING",
    LoaderState.PAUSED: "PAUSED",
    LoaderState.ERROR: "ERROR",
}

ERROR_NAMES = {
    LoaderError.NONE: "NONE",
    LoaderError.OVERCURRENT: "OCP",
    LoaderError.OVERTEMPERATURE: "OTP",
    LoaderError.UNDERVOLTAGE: "UVP",
}

MODE_NAMES = {
    LoaderMode.CC: "CC",
    LoaderMode.CV: "CV",
    LoaderMode.CP: "CP",
    LoaderMode.CR: "CR",
}

STATE_BY_NAME = {v: k for k, v in STATE_NAMES.items()}
ERROR_BY_NAME = {v: k for k, v in ERROR_NAMES.items()}
MODE_BY_NAME = {v: k for k, v in MODE_NAMES.items()}

# Per-mode display metadata: unit suffix and the setpoint's limit/default.
MODE_META = {
    LoaderMode.CC: {
        "unit": "A",
        "label": "Current",
        "label_cn": "电流",
        "limit": CURRENT_MAX,
        "default": DEFAULT_CURRENT_SETPOINT,
    },
    LoaderMode.CV: {
        "unit": "V",
        "label": "Voltage",
        "label_cn": "电压",
        "limit": VOLTAGE_MAX,
        "default": DEFAULT_VOLTAGE_SETPOINT,
    },
    LoaderMode.CP: {
        "unit": "W",
        "label": "Power",
        "label_cn": "功率",
        "limit": POWER_MAX,
        "default": DEFAULT_POWER_SETPOINT,
    },
    LoaderMode.CR: {
        "unit": "ohm",
        "label": "Resistance",
        "label_cn": "电阻",
        "limit": RESISTANCE_MAX,
        "default": DEFAULT_RESISTANCE_SETPOINT,
    },
}

# exit_code_t (lib/tools/common_def.h) — used to explain : FAIL responses
EXIT_CODES = {
    0: "EXIT_OK",
    -1: "EXIT_FAIL",
    -2: "EXIT_TIMEOUT",
    -3: "EXIT_INVALID_PARAM",
    -4: "EXIT_NOT_SUPPORTED",
    -5: "EXIT_NO_MEMORY",
    -6: "EXIT_BUSY",
    -7: "EXIT_NO_RESOURCE",
    -8: "EXIT_ALREADY_EXISTS",
    -9: "EXIT_DOES_NOT_EXIST",
    -10: "EXIT_NOT_INITIALIZED",
    -11: "EXIT_ALREADY_INITIALIZED",
    -12: "EXIT_CRC_MISMATCH",
    -13: "EXIT_HW_FAILURE",
    -14: "EXIT_UNKNOWN",
}

# loader_core_request_run() returns EXIT_BUSY only when state == ERROR.
EXIT_BUSY = -6
