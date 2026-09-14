"""Data model shared by the transport, protocol and UI layers.

`LoaderSnapshot` is the host-side mirror of `loader_runtime_t` plus the
actuator / fan readback that `lstatus` exposes. Fields the firmware cannot
report stay at their sentinel (``None``) so the UI can show "—" instead of
inventing a zero.
"""

from __future__ import annotations

import math
import time
from dataclasses import asdict, dataclass, field, replace

from .config import LoaderError, LoaderMode, LoaderState

# Sentinel for "the device has not told us this yet".
UNKNOWN = math.nan


def _is_known(value: float | None) -> bool:
    return value is not None and not math.isnan(value)


@dataclass
class LoaderSnapshot:
    """One coherent view of the device.

    Setpoints (``*_setpoint``) are the clamped values the firmware echoed back,
    not what the user typed. Measurements come straight from the CLI text.

    NOTE: with LOADER_USE_SOFTWARE_CURRENT_PID == 0 the firmware's
    ``current_measurement`` is the *commanded* current (load_out_get_current),
    not an ADC sample. `current_is_commanded` records that so the UI can label
    it honestly.
    """

    state: LoaderState | None = None
    error: LoaderError | None = None
    mode: LoaderMode | None = None

    current_setpoint: float | None = None
    voltage_setpoint: float | None = None
    power_setpoint: float | None = None
    resistance_setpoint: float | None = None

    current_measurement: float | None = None
    voltage_measurement: float | None = None
    power_measurement: float | None = None
    resistance_measurement: float | None = None
    temperature_measurement: float | None = None

    out_enabled: bool | None = None
    out_current: float | None = None
    out_voltage: float | None = None
    out_norm: float | None = None

    fan_enabled: bool | None = None
    fan_speed: float | None = None  # fraction 0..1
    fan_target: float | None = None  # fraction 0..1

    ocp_limit: float | None = None
    otp_limit: float | None = None
    current_max: float | None = None

    current_is_commanded: bool = True

    # Monotonic receive time, used for the log and for CSV timestamps.
    rx_time: float = field(default_factory=time.monotonic)

    def merged_with(self, newer: LoaderSnapshot) -> LoaderSnapshot:
        """Overlay `newer`'s known fields onto self (partial-update merge).

        `lmeas` only carries measurements and `lstatus` carries everything, so
        the UI keeps one accumulated snapshot and folds each reply into it.
        """
        out = replace(self)
        for key, value in asdict(newer).items():
            if key == "rx_time":
                out.rx_time = newer.rx_time
                continue
            if key == "current_is_commanded":
                out.current_is_commanded = newer.current_is_commanded
                continue
            if value is None:
                continue
            if isinstance(value, float) and math.isnan(value):
                continue
            setattr(out, key, value)
        return out

    @property
    def active_setpoint(self) -> float | None:
        """Setpoint of the currently selected mode (what `active :` reports)."""
        match self.mode:
            case LoaderMode.CC:
                return self.current_setpoint
            case LoaderMode.CV:
                return self.voltage_setpoint
            case LoaderMode.CP:
                return self.power_setpoint
            case LoaderMode.CR:
                return self.resistance_setpoint
            case _:
                return None

    @property
    def is_faulted(self) -> bool:
        return self.state == LoaderState.ERROR

    def has_any_measurement(self) -> bool:
        return any(
            _is_known(v)
            for v in (
                self.current_measurement,
                self.voltage_measurement,
                self.power_measurement,
                self.resistance_measurement,
                self.temperature_measurement,
            )
        )


@dataclass
class CommandResult:
    """Outcome of a control command (`lrun`, `lset`, ...)."""

    command: str
    ok: bool
    detail: str = ""
    exit_code: int | None = None
    snapshot: LoaderSnapshot | None = None
