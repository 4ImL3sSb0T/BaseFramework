"""ASCII (Letter Shell) command construction and tolerant reply parsing.

The device side is `src/app/loader_cli.c`, which prints via `logPrintln` --
every line terminates with CRLF and is produced by a fixed printf format.

Why parsing is deliberately lenient
-----------------------------------
USART1 carries a *human* console, not a framed protocol:

* the shell echoes every typed byte back (`shellInsertByte` calls
  `shellWriteByte` unconditionally), so replies are interleaved with a copy of
  our own command text;
* the prompt ``letter:/$ `` is written WITHOUT a trailing newline, so it gets
  prefixed onto whatever the next reply is;
* `SHELL_SHOW_INFO=1` prints an ASCII banner on connect, and
  `LOG_USING_COLOR=1` / `SHELL_CLS_WHEN_LOGIN=1` emit ANSI escape sequences.

Therefore: strip ANSI, strip prompt tokens, then match known formats
anchored at both ends. Anything that does not match is surfaced as plain text
for the terminal panel and otherwise ignored. There is no strict resync, which
is what makes a noisy console usable.
"""

from __future__ import annotations

import re
from dataclasses import dataclass

from .config import (
    ERROR_BY_NAME,
    MODE_BY_NAME,
    MODE_NAMES,
    STATE_BY_NAME,
    LoaderMode,
)
from .model import CommandResult, LoaderSnapshot

# ---------------------------------------------------------------------------
# Command construction
# ---------------------------------------------------------------------------

# `lmeas\n` is sufficient: shell_cfg.h enables SHELL_ENTER_LF (and CRLF), and a
# lone LF unambiguously terminates a command. `SHELL_ENTER_CR` is disabled, so
# never send a bare CR.
_LINE_END = "\n"


def build_command(name: str, *args: object) -> bytes:
    """Build a raw shell command line, appending the terminator."""
    parts = [str(name), *(str(a) for a in args)]
    return (" ".join(parts) + _LINE_END).encode("ascii", errors="replace")


def cmd_meas() -> bytes:
    return build_command("lmeas")


def cmd_status() -> bytes:
    return build_command("lstatus")


def cmd_out() -> bytes:
    return build_command("lout")


def cmd_run() -> bytes:
    return build_command("lrun")


def cmd_stop() -> bytes:
    return build_command("lstop")


def cmd_clear_fault() -> bytes:
    return build_command("lclr")


def cmd_mode(mode: LoaderMode) -> bytes:
    return build_command("lmode", MODE_NAMES[mode].lower())


def cmd_mode_query() -> bytes:
    return build_command("lmode")


def cmd_set(value: float, channel: str | None = None, *, decimals: int = 3) -> bytes:
    """`lset [i|v|p|r] <value>`; channel omitted means "active mode"."""
    text = f"{value:.{decimals}f}"
    if channel is None:
        return build_command("lset", text)
    return build_command("lset", channel, text)


def cmd_fan_on() -> bytes:
    return build_command("lfan", "on")


def cmd_fan_off() -> bytes:
    return build_command("lfan", "off")


def cmd_fan_percent(percent: float) -> bytes:
    return build_command("lfan", f"{percent:.0f}")


def cmd_fan_query() -> bytes:
    return build_command("lfan")


def cmd_help() -> bytes:
    return build_command("lhelp")


# ---------------------------------------------------------------------------
# Reply channel metadata
# ---------------------------------------------------------------------------

CHANNEL_FOR_COMMAND = {
    "lmeas": "lmeas",
    "lstatus": "lstatus",
    "lout": "lout",
}

# Setpoint channel letter per mode, as accepted by `lset`.
SETPOINT_CHANNEL = {
    LoaderMode.CC: "i",
    LoaderMode.CV: "v",
    LoaderMode.CP: "p",
    LoaderMode.CR: "r",
}


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------

# Numbers as produced by %f: sign, digits, optional fraction, optional exponent.
_NUM = r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?"

_ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[ -/]*[@-~]")
_OTHER_ESC_RE = re.compile(r"\x1b[@-Z\\-_]")

# Prompt is `username:path$ ` -- path is "/" because shellSetPath() is never
# called, giving `letter:/$ `. Tolerate other paths / a trailing space missing.
_PROMPT_RE = re.compile(r"[A-Za-z0-9_.\-]{1,32}:[^\s$]{0,64}\$\s?")

_CTRL_RE = re.compile(r"[\x00-\x08\x0b\x0c\x0e-\x1f]")


def strip_noise(line: str) -> str:
    """Remove ANSI sequences, control bytes and shell prompts; trim spaces."""
    line = _ANSI_RE.sub("", line)
    line = _OTHER_ESC_RE.sub("", line)
    line = _CTRL_RE.sub("", line)
    line = _PROMPT_RE.sub(" ", line)
    return line.strip()


@dataclass
class Event:
    """One thing learned from the wire."""

    kind: str  # "snapshot" | "result" | "text"
    text: str = ""
    snapshot: LoaderSnapshot | None = None
    result: CommandResult | None = None


class Parsed:
    """Regex table entry: (compiled pattern, handler name)."""

    __slots__ = ("kind", "pattern")

    def __init__(self, pattern: re.Pattern[str], kind: str) -> None:
        self.pattern = pattern
        self.kind = kind


def _re(body: str) -> re.Pattern[str]:
    return re.compile(body)


class AsciiParser:
    """Incremental line parser for the loader CLI's ASCII replies."""

    def __init__(self) -> None:
        self._buf = ""
        self._patterns: list[Parsed] = self._build_patterns()
        # Counts for the diagnostics panel.
        self.lines_seen = 0
        self.lines_matched = 0

    # -- patterns ----------------------------------------------------------

    def _build_patterns(self) -> list[Parsed]:
        # Order matters: most specific (labelled) forms first, then the
        # unlabelled single-line forms.
        return [
            Parsed(_re(r"^---\s*loader status\s*---$"), "marker"),
            # lstatus labelled fields
            Parsed(_re(r"^state\s*:\s*(\S+)$"), "state"),
            Parsed(_re(r"^error\s*:\s*(\S+)$"), "error"),
            Parsed(_re(r"^mode\s*:\s*(\S+)$"), "mode"),
            Parsed(
                _re(rf"^set\s*:\s*I=({_NUM})A\s+V=({_NUM})V\s+P=({_NUM})W\s+R=({_NUM})ohm$"),
                "set",
            ),
            Parsed(_re(rf"^active\s*:\s*({_NUM})\s+(\S+)$"), "active"),
            Parsed(
                _re(
                    rf"^meas\s*:\s*I=({_NUM})A\s+V=({_NUM})V\s+P=({_NUM})W"
                    rf"\s+R=({_NUM})ohm\s+T=({_NUM})C$"
                ),
                "meas",
            ),
            Parsed(
                _re(
                    rf"^out\s*:\s*en=(\d+)\s+Iref=({_NUM})A\s+Vref=({_NUM})V"
                    rf"\s+norm=({_NUM})$"
                ),
                "out",
            ),
            Parsed(
                _re(rf"^fan\s*:\s*en=(\d+)\s+speed=({_NUM})%\s+target=({_NUM})%$"),
                "fan",
            ),
            Parsed(
                _re(rf"^limits\s*:\s*OCP=({_NUM})A\s+OTP=({_NUM})C\s+Imax=({_NUM})A$"),
                "limits",
            ),
            # `lmeas` standalone: single spaces, no label.
            Parsed(
                _re(
                    rf"^I=({_NUM})A\s+V=({_NUM})V\s+P=({_NUM})W\s+R=({_NUM})ohm"
                    rf"\s+T=({_NUM})C$"
                ),
                "meas_bare",
            ),
            # `lout` standalone (has the "(raw scale N)" trailer)
            Parsed(
                _re(
                    rf"^load_out\s*:\s*en=(\d+)\s+Iref=({_NUM})A\s+Vref=({_NUM})V"
                    rf"\s+norm=({_NUM})\s+\(raw scale (\d+)\)$"
                ),
                "out_bare",
            ),
            # `lset` success echo
            Parsed(
                _re(
                    rf"^lset\s*:\s*OK\s+I=({_NUM})A\s+V=({_NUM})V\s+P=({_NUM})W"
                    rf"\s+R=({_NUM})ohm$"
                ),
                "set_ok",
            ),
            # `lmode` results
            Parsed(_re(rf"^lmode\s*:\s*OK\s*->\s*(\S+)\s+set=({_NUM})(\S*)$"), "mode_ok"),
            Parsed(_re(r"^lmode\s*:\s*bad mode\s*'([^']*)'.*$"), "mode_bad"),
            Parsed(_re(r"^mode=(\S+)\s+\(use:.*$"), "mode_query"),
            # `lrun` / `lclr` continuation lines
            Parsed(_re(rf"^state=(\S+)\s+mode=(\S+)\s+set=({_NUM})(\S*)$"), "run_cont"),
            Parsed(_re(r"^state=(\S+)\s+error=(\S+)$"), "clr_cont"),
            Parsed(_re(rf"^now mode=(\S+)\s+set=({_NUM})(\S*)$"), "usage_cont"),
            # bare `: OK` / `: FAIL <NAME> (<code>)` results
            Parsed(
                _re(r"^(lrun|lstop|lclr|lfan on|lfan off|lfan enable|lfan)\s*:\s*OK$"),
                "result_ok",
            ),
            Parsed(
                _re(
                    r"^(lrun|lstop|lclr|lfan on|lfan off|lfan enable|lfan)"
                    r"\s*:\s*FAIL\s+(\S+)\s+\((-?\d+)\)$"
                ),
                "result_fail",
            ),
            # error/usage strings worth showing verbatim
            Parsed(_re(r"^lset\s*:\s*bad number\s*'([^']*)'$"), "set_bad_number"),
            Parsed(
                _re(
                    r"^(lset\s*:\s*channel must be i\|v\|p\|r|usage: lset.*"
                    r"|lfan\s*:\s*use on\|off.*|usage: lfan.*)$"
                ),
                "usage",
            ),
            # `lfan` speed echo
            Parsed(_re(rf"^speed=({_NUM})%$"), "fan_speed"),
            # `setVar`-style indented continuation for fan
            Parsed(_re(r"^$"), "blank"),
        ]

    # -- feeding -----------------------------------------------------------

    def feed(self, data: bytes) -> list[Event]:
        """Feed raw bytes; return the events they produced."""
        self._buf += data.decode("utf-8", errors="replace")
        events: list[Event] = []

        parts = re.split(r"\r\n|\n|\r", self._buf)
        self._buf = parts.pop()  # trailing incomplete line stays buffered

        for raw in parts:
            events.extend(self._handle_line(raw))
        return events

    def flush(self) -> list[Event]:
        """Emit a buffered partial line (e.g. on disconnect)."""
        if not self._buf:
            return []
        buf, self._buf = self._buf, ""
        return self._handle_line(buf)

    def _handle_line(self, raw: str) -> list[Event]:
        self.lines_seen += 1
        clean = strip_noise(raw)
        if not clean:
            return []

        for entry in self._patterns:
            match = entry.pattern.match(clean)
            if match is None:
                continue
            self.lines_matched += 1
            if entry.kind == "blank":
                return []
            events = self._apply(entry.kind, match, clean)
            if events is not None:
                return events
            # Recognised but produces no event (e.g. marker) -> still text.
            break

        # Unrecognised (echo, banner, help text): forward as plain text so the
        # terminal panel can show it, and let the caller ignore it otherwise.
        return [Event(kind="text", text=clean)]

    # -- handlers ----------------------------------------------------------

    def _apply(self, kind: str, m: re.Match[str], clean: str) -> list[Event] | None:
        snap = LoaderSnapshot()
        events: list[Event] = []

        if kind == "marker":
            # Pure decoration ("--- loader status ---"). Suppressed so that
            # polled status replies do not spam the terminal panel; all real
            # fields are turned into snapshots instead.
            return []

        if kind == "state":
            state = STATE_BY_NAME.get(m.group(1))
            if state is None:
                return [Event(kind="text", text=clean)]
            snap.state = state

        elif kind == "error":
            err = ERROR_BY_NAME.get(m.group(1))
            if err is None:
                return [Event(kind="text", text=clean)]
            snap.error = err

        elif kind == "mode":
            mode = MODE_BY_NAME.get(m.group(1))
            if mode is None:
                return [Event(kind="text", text=clean)]
            snap.mode = mode

        elif kind == "set":
            snap.current_setpoint = float(m.group(1))
            snap.voltage_setpoint = float(m.group(2))
            snap.power_setpoint = float(m.group(3))
            snap.resistance_setpoint = float(m.group(4))

        elif kind == "active":
            # Redundant with mode+setpoints; recognised so it is not treated as
            # an unknown line. Value is cross-checked by the UI if needed.
            return []

        elif kind == "meas" or kind == "meas_bare":
            snap.current_measurement = float(m.group(1))
            snap.voltage_measurement = float(m.group(2))
            snap.power_measurement = float(m.group(3))
            snap.resistance_measurement = float(m.group(4))
            snap.temperature_measurement = float(m.group(5))

        elif kind == "out":
            snap.out_enabled = bool(int(m.group(1)))
            snap.out_current = float(m.group(2))
            snap.out_voltage = float(m.group(3))
            snap.out_norm = float(m.group(4))

        elif kind == "out_bare":
            snap.out_enabled = bool(int(m.group(1)))
            snap.out_current = float(m.group(2))
            snap.out_voltage = float(m.group(3))
            snap.out_norm = float(m.group(4))
            events.append(Event(kind="text", text=clean))
            events.append(Event(kind="snapshot", text=clean, snapshot=snap))
            return events

        elif kind == "fan":
            snap.fan_enabled = bool(int(m.group(1)))
            snap.fan_speed = float(m.group(2)) / 100.0
            snap.fan_target = float(m.group(3)) / 100.0

        elif kind == "limits":
            snap.ocp_limit = float(m.group(1))
            snap.otp_limit = float(m.group(2))
            snap.current_max = float(m.group(3))

        elif kind == "set_ok":
            snap.current_setpoint = float(m.group(1))
            snap.voltage_setpoint = float(m.group(2))
            snap.power_setpoint = float(m.group(3))
            snap.resistance_setpoint = float(m.group(4))
            # Emit both: the snapshot carries data, the result carries intent.
            events.append(Event(kind="snapshot", text=clean, snapshot=snap))
            events.append(
                Event(
                    kind="result",
                    text=clean,
                    snapshot=snap,
                    result=CommandResult(
                        command="lset",
                        ok=True,
                        detail=clean,
                        snapshot=snap,
                    ),
                )
            )
            return events

        elif kind == "mode_ok":
            mode = MODE_BY_NAME.get(m.group(1))
            if mode is not None:
                snap.mode = mode
                _assign_active_setpoint(snap, mode, float(m.group(2)))
            events.append(Event(kind="snapshot", text=clean, snapshot=snap))
            events.append(
                Event(
                    kind="result",
                    text=clean,
                    snapshot=snap,
                    result=CommandResult(command="lmode", ok=True, detail=clean, snapshot=snap),
                )
            )
            return events

        elif kind == "mode_bad":
            events.append(
                Event(
                    kind="result",
                    text=clean,
                    result=CommandResult(command="lmode", ok=False, detail=clean),
                )
            )
            return events

        elif kind == "mode_query":
            mode = MODE_BY_NAME.get(m.group(1))
            if mode is not None:
                snap.mode = mode

        elif kind == "run_cont":
            state = STATE_BY_NAME.get(m.group(1))
            mode = MODE_BY_NAME.get(m.group(2))
            if state is not None:
                snap.state = state
            if mode is not None:
                snap.mode = mode
            value = float(m.group(3))
            if mode is not None:
                _assign_active_setpoint(snap, mode, value)

        elif kind == "clr_cont":
            state = STATE_BY_NAME.get(m.group(1))
            err = ERROR_BY_NAME.get(m.group(2))
            if state is not None:
                snap.state = state
            if err is not None:
                snap.error = err

        elif kind == "usage_cont":
            mode = MODE_BY_NAME.get(m.group(1))
            if mode is not None:
                snap.mode = mode
                _assign_active_setpoint(snap, mode, float(m.group(2)))
            events.append(Event(kind="text", text=clean))
            events.append(Event(kind="snapshot", text=clean, snapshot=snap))
            return events

        elif kind == "result_ok":
            name = m.group(1)
            events.append(
                Event(
                    kind="result",
                    text=clean,
                    result=CommandResult(command=name, ok=True, detail=clean),
                )
            )
            return events

        elif kind == "result_fail":
            name = m.group(1)
            code = int(m.group(3))
            events.append(
                Event(
                    kind="result",
                    text=clean,
                    result=CommandResult(
                        command=name,
                        ok=False,
                        detail=clean,
                        exit_code=code,
                    ),
                )
            )
            return events

        elif kind == "set_bad_number":
            events.append(
                Event(
                    kind="result",
                    text=clean,
                    result=CommandResult(command="lset", ok=False, detail=clean),
                )
            )
            return events

        elif kind == "usage":
            events.append(Event(kind="text", text=clean))
            return events

        elif kind == "fan_speed":
            snap.fan_speed = float(m.group(1)) / 100.0

        else:  # pragma: no cover - defensive
            return [Event(kind="text", text=clean)]

        events.append(Event(kind="snapshot", text=clean, snapshot=snap))
        return events


def _assign_active_setpoint(snap: LoaderSnapshot, mode: LoaderMode, value: float) -> None:
    if mode == LoaderMode.CC:
        snap.current_setpoint = value
    elif mode == LoaderMode.CV:
        snap.voltage_setpoint = value
    elif mode == LoaderMode.CP:
        snap.power_setpoint = value
    elif mode == LoaderMode.CR:
        snap.resistance_setpoint = value


__all__ = [
    "CHANNEL_FOR_COMMAND",
    "SETPOINT_CHANNEL",
    "AsciiParser",
    "Event",
    "build_command",
    "cmd_clear_fault",
    "cmd_fan_off",
    "cmd_fan_on",
    "cmd_fan_percent",
    "cmd_fan_query",
    "cmd_help",
    "cmd_meas",
    "cmd_mode",
    "cmd_mode_query",
    "cmd_out",
    "cmd_run",
    "cmd_set",
    "cmd_status",
    "cmd_stop",
    "strip_noise",
]
