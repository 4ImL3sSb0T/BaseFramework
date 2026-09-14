"""CSV recording, export and replay.

Long-session logging needs to survive a crash, so rows are flushed as they are
written rather than buffered until close.

The CSV schema is the flat set of snapshot fields plus a timestamp, which makes
it both human-inspectable and directly re-loadable for replay.
"""

from __future__ import annotations

import csv
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Self

from .config import ERROR_NAMES, MODE_NAMES, STATE_NAMES
from .model import LoaderSnapshot

COLUMNS = [
    "t_rel_s",
    "wall_time",
    "state",
    "error",
    "mode",
    "current_setpoint",
    "voltage_setpoint",
    "power_setpoint",
    "resistance_setpoint",
    "current_a",
    "voltage_v",
    "power_w",
    "resistance_ohm",
    "temperature_c",
    "out_enabled",
    "out_current_a",
    "out_voltage_v",
    "out_norm",
    "fan_enabled",
    "fan_speed_pct",
    "fan_target_pct",
]


def _fmt(value: float | None, decimals: int = 4) -> str:
    if value is None:
        return ""
    return f"{value:.{decimals}f}"


class CsvRecorder:
    """Append snapshots to a CSV file, one row per update."""

    def __init__(self, path: str | Path, min_interval_s: float = 0.0) -> None:
        self.path = Path(path)
        self.min_interval_s = min_interval_s
        self._file = None
        self._writer = None
        self._t0 = time.monotonic()
        self._last_write = 0.0
        self.row_count = 0

    @property
    def is_recording(self) -> bool:
        return self._file is not None

    def start(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self._file = self.path.open("w", newline="", encoding="utf-8")
        self._writer = csv.writer(self._file)
        self._writer.writerow(COLUMNS)
        self._file.flush()
        self._t0 = time.monotonic()
        self._last_write = 0.0
        self.row_count = 0

    def write(self, snap: LoaderSnapshot, *, force: bool = False) -> None:
        if self._writer is None:
            return
        now = time.monotonic()
        if not force and self.min_interval_s > 0 and (now - self._last_write) < self.min_interval_s:
            return
        self._last_write = now

        self._writer.writerow(
            [
                f"{now - self._t0:.3f}",
                time.strftime("%Y-%m-%d %H:%M:%S"),
                STATE_NAMES.get(snap.state, "") if snap.state is not None else "",
                ERROR_NAMES.get(snap.error, "") if snap.error is not None else "",
                MODE_NAMES.get(snap.mode, "") if snap.mode is not None else "",
                _fmt(snap.current_setpoint, 3),
                _fmt(snap.voltage_setpoint, 3),
                _fmt(snap.power_setpoint, 3),
                _fmt(snap.resistance_setpoint, 3),
                _fmt(snap.current_measurement, 4),
                _fmt(snap.voltage_measurement, 4),
                _fmt(snap.power_measurement, 4),
                _fmt(snap.resistance_measurement, 4),
                _fmt(snap.temperature_measurement, 2),
                "" if snap.out_enabled is None else int(snap.out_enabled),
                _fmt(snap.out_current, 4),
                _fmt(snap.out_voltage, 4),
                _fmt(snap.out_norm, 5),
                "" if snap.fan_enabled is None else int(snap.fan_enabled),
                _fmt(None if snap.fan_speed is None else snap.fan_speed * 100.0, 1),
                _fmt(None if snap.fan_target is None else snap.fan_target * 100.0, 1),
            ]
        )
        self.row_count += 1
        # Flush per row: a long capture must not lose everything on a crash.
        self._file.flush()

    def stop(self) -> None:
        if self._file is not None:
            self._file.close()
        self._file = None
        self._writer = None

    def __enter__(self) -> Self:
        self.start()
        return self

    def __exit__(self, *exc) -> None:
        self.stop()


@dataclass
class ReplayData:
    """A loaded CSV, reshaped into per-channel series for plotting."""

    t: list[float]
    channels: dict[str, list[float | None]]


def load_csv(path: str | Path) -> ReplayData:
    """Load a recorded CSV for replay.

    Missing cells become ``None`` so gaps stay gaps instead of reading as 0.
    """

    def to_float(text: str) -> float | None:
        text = (text or "").strip()
        if not text:
            return None
        try:
            return float(text)
        except ValueError:
            return None

    t: list[float] = []
    series: dict[str, list[float | None]] = {
        "current": [],
        "voltage": [],
        "power": [],
        "resistance": [],
        "temperature": [],
    }

    with Path(path).open("r", newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            t.append(to_float(row.get("t_rel_s", "")) or 0.0)
            series["current"].append(to_float(row.get("current_a", "")))
            series["voltage"].append(to_float(row.get("voltage_v", "")))
            series["power"].append(to_float(row.get("power_w", "")))
            series["resistance"].append(to_float(row.get("resistance_ohm", "")))
            series["temperature"].append(to_float(row.get("temperature_c", "")))

    return ReplayData(t=t, channels=series)


__all__ = ["COLUMNS", "CsvRecorder", "ReplayData", "load_csv"]
