"""Transports: real serial port and a built-in device simulator.

Both implement the same tiny duck-typed interface so the client cannot tell
them apart:

    open() -> None
    close() -> None
    read(max_bytes: int, timeout_s: float) -> bytes
    write(data: bytes) -> None
    is_open -> bool
    description -> str

`SimulatorTransport` deliberately reproduces the *annoying* parts of the real
console -- character echo, the prompt glued onto the next reply, and the
startup banner -- so the ASCII parser's tolerance is exercised end to end
rather than only in unit tests.
"""

from __future__ import annotations

import threading
import time
from collections import deque

from . import ascii_proto, config
from .config import ERROR_NAMES, MODE_NAMES, STATE_NAMES, LoaderError, LoaderMode, LoaderState
from .model import LoaderSnapshot


class TransportError(RuntimeError):
    pass


# ---------------------------------------------------------------------------
# Real serial port
# ---------------------------------------------------------------------------


class SerialTransport:
    """Thin wrapper over pyserial with a non-blocking read."""

    def __init__(
        self,
        port: str,
        baudrate: int = config.DEFAULT_BAUDRATE,
        read_timeout_s: float = 0.05,
    ) -> None:
        self.port = port
        self.baudrate = baudrate
        self._read_timeout = read_timeout_s
        self._serial = None

    def open(self) -> None:
        import serial  # imported lazily so tests need no hardware stack

        try:
            self._serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=self._read_timeout,
                write_timeout=1.0,
            )
        except Exception as exc:  # serial.SerialException et al.
            raise TransportError(f"{self.port}: {exc}") from exc

    def close(self) -> None:
        if self._serial is not None:
            try:
                self._serial.close()
            finally:
                self._serial = None

    @property
    def is_open(self) -> bool:
        return self._serial is not None and self._serial.is_open

    @property
    def description(self) -> str:
        return f"{self.port} @ {self.baudrate}"

    def read(self, max_bytes: int = 4096, timeout_s: float | None = None) -> bytes:
        if self._serial is None:
            return b""
        if timeout_s is not None:
            self._serial.timeout = timeout_s
        try:
            waiting = self._serial.in_waiting
        except Exception:
            waiting = 0
        return self._serial.read(max(1, min(max_bytes, waiting or 1)))

    def write(self, data: bytes) -> None:
        if self._serial is None:
            return
        try:
            self._serial.write(data)
        except Exception as exc:
            raise TransportError(f"write failed: {exc}") from exc


def list_serial_ports() -> list[tuple[str, str]]:
    """Return [(device, human-readable description)] for available ports."""
    try:
        from serial.tools import list_ports
    except Exception:
        return []
    out: list[tuple[str, str]] = []
    for info in list_ports.comports():
        label = info.description or "serial port"
        if info.device and info.device not in label:
            label = f"{label} ({info.device})"
        out.append((info.device, label))
    return sorted(out, key=lambda item: item[0])


# ---------------------------------------------------------------------------
# Simulator
# ---------------------------------------------------------------------------

# Firmware output formats, mirrored from src/app/loader_cli.c.
# SHELL_SHOW_INFO=1 prints a banner (with ANSI colour) on connect, and the
# prompt carries no trailing newline -- both are reproduced on purpose.
_BANNER = (
    "\r\n"
    "\x1b[32mBaseFramework H750\x1b[0m\r\n"
    f"Build: {time.strftime('%b %d %Y %H:%M:%S')}\r\n"
    "letter shell 3.2.4\r\n"
)
_PROMPT = "letter:/$ "

_HELP_LINES = [
    "loader CLI:",
    "  lhelp              this help",
    "  lstatus            state / mode / set / meas / limits",
    "  lmeas              measurements only",
    "  lrun               request RUN (enable output)",
    "  lstop              request STOP (disable output)",
    "  lclr               clear FAULT -> IDLE",
    "  lmode [cc|cv|cp|cr] get/set mode",
    "  lset [i|v|p|r] <x> set setpoint (channel optional)",
    "  lset <x>           set active-mode setpoint",
    "  lout               actuator readback (norm/Iref/en)",
    "  lfan [on|off|%%]    fan enable / duty percent 0..100",
]


class SimulatedDevice:
    """A rough behavioural model of the electronic load.

    Not a physics simulation: it exists to make the GUI move plausibly and to
    service both the ASCII and binary protocols.
    """

    def __init__(self) -> None:
        self.state = LoaderState.IDLE
        self.error = LoaderError.NONE
        self.mode = LoaderMode.CC
        self.current_setpoint = config.DEFAULT_CURRENT_SETPOINT
        self.voltage_setpoint = config.DEFAULT_VOLTAGE_SETPOINT
        self.power_setpoint = config.DEFAULT_POWER_SETPOINT
        self.resistance_setpoint = config.DEFAULT_RESISTANCE_SETPOINT

        # Physical-ish quantities.
        self._voltage = 0.0
        self._current = 0.0
        self._temperature = 25.0
        self._fan_enabled = False
        self._fan_target = 0.0

    # -- simulation --------------------------------------------------------

    def step(self, dt: float) -> None:
        """Advance the model; called on every simulator poll."""
        # Source voltage: a fixed bench supply that droops slightly under load.
        source_v = 12.0
        r_source = 0.15

        if self.state == LoaderState.RUNNING:
            target_i = self._target_current()
            self._current += (target_i - self._current) * min(1.0, dt * 8.0)
        else:
            self._current += (0.0 - self._current) * min(1.0, dt * 8.0)
            self._current = max(0.0, self._current)

        self._current = max(0.0, min(config.CURRENT_MAX, self._current))
        self._voltage = max(0.0, source_v - self._current * r_source)

        # Thermal model: I^2R heating vs. fan cooling, ambient 25 degC.
        heat = self._current * self._current * 0.35
        fan_duty = self._fan_target if self._fan_enabled else 0.0
        cooling = (self._temperature - 25.0) * (0.06 + 0.18 * fan_duty)
        self._temperature += (heat - cooling) * dt
        self._temperature = max(25.0, min(120.0, self._temperature))

        # Latch the software protections the firmware runs at 5 ms.
        if self.state == LoaderState.RUNNING:
            if self._current > config.OVERCURRENT_LIMIT:
                self._fault(LoaderError.OVERCURRENT)
            elif self._temperature > config.OVERTEMPERATURE_LIMIT:
                self._fault(LoaderError.OVERTEMPERATURE)

    def _target_current(self) -> float:
        v = self._voltage
        if self.mode == LoaderMode.CC:
            return self.current_setpoint
        if self.mode == LoaderMode.CV:
            # Crude P-ish controller towards the voltage setpoint.
            err = self.voltage_setpoint - v
            return max(0.0, min(config.CURRENT_MAX, err * 1.2))
        if self.mode == LoaderMode.CP:
            return min(config.CURRENT_MAX, self.power_setpoint / max(v, 0.05))
        if self.mode == LoaderMode.CR:
            return min(config.CURRENT_MAX, v / max(self.resistance_setpoint, 0.01))
        return 0.0

    def _fault(self, error: LoaderError) -> None:
        self.state = LoaderState.ERROR
        self.error = error
        self._current = 0.0

    # -- derived readouts --------------------------------------------------

    @property
    def out_enabled(self) -> bool:
        return self.state == LoaderState.RUNNING

    @property
    def out_current(self) -> float:
        return self._current if self.out_enabled else 0.0

    @property
    def out_voltage(self) -> float:
        # load_out_get_voltage() = norm * 3.3, norm = I / 6.6 A full scale.
        return self.out_current / (config.OUT_VOLTAGE_MAX / 0.5)

    @property
    def out_norm(self) -> float:
        return self.out_current / (config.OUT_VOLTAGE_MAX / 0.5)

    @property
    def power(self) -> float:
        return self._current * self._voltage

    @property
    def resistance(self) -> float:
        if abs(self._current) > 0.001:
            return self._voltage / self._current
        return 0.0

    def snapshot(self) -> LoaderSnapshot:
        return LoaderSnapshot(
            state=self.state,
            error=self.error,
            mode=self.mode,
            current_setpoint=self.current_setpoint,
            voltage_setpoint=self.voltage_setpoint,
            power_setpoint=self.power_setpoint,
            resistance_setpoint=self.resistance_setpoint,
            current_measurement=self.out_current,
            voltage_measurement=self._voltage,
            power_measurement=self.power,
            resistance_measurement=self.resistance,
            temperature_measurement=self._temperature,
            out_enabled=self.out_enabled,
            out_current=self.out_current,
            out_voltage=self.out_voltage,
            out_norm=self.out_norm,
            fan_enabled=self._fan_enabled,
            fan_speed=self._fan_target if self._fan_enabled else 0.0,
            fan_target=self._fan_target,
            ocp_limit=config.OVERCURRENT_LIMIT,
            otp_limit=config.OVERTEMPERATURE_LIMIT,
            current_max=config.CURRENT_MAX,
            current_is_commanded=True,
        )

    # -- command handling (ASCII) -----------------------------------------

    def handle_command(self, line: str) -> list[str]:
        """Execute one shell line; return the reply lines (no line endings)."""
        tokens = line.split()
        if not tokens:
            return []
        name, args = tokens[0], tokens[1:]

        handler = getattr(self, f"_cmd_{name}", None)
        if handler is None:
            return [f"{name}: command not found"]
        return handler(args)

    def _cmd_lhelp(self, args: list[str]) -> list[str]:
        return [
            *_HELP_LINES,
            "limits: I<=%.2fA V<=%.2fV P<=%.2fW R<=%.1fohm OCP=%.2fA OTP=%.0fC"
            % (
                config.CURRENT_MAX,
                config.VOLTAGE_MAX,
                config.POWER_MAX,
                config.RESISTANCE_MAX,
                config.OVERCURRENT_LIMIT,
                config.OVERTEMPERATURE_LIMIT,
            ),
        ]

    def _cmd_lstatus(self, args: list[str]) -> list[str]:
        set_v, unit = self._active()
        return [
            "--- loader status ---",
            f"state  : {STATE_NAMES[self.state]}",
            f"error  : {ERROR_NAMES[self.error]}",
            f"mode   : {MODE_NAMES[self.mode]}",
            "set    : I=%.3fA  V=%.3fV  P=%.3fW  R=%.3fohm"
            % (
                self.current_setpoint,
                self.voltage_setpoint,
                self.power_setpoint,
                self.resistance_setpoint,
            ),
            "active : %.3f %s" % (set_v, unit),
            "meas   : I=%.3fA  V=%.3fV  P=%.3fW  R=%.3fohm  T=%.1fC"
            % (self.out_current, self._voltage, self.power, self.resistance, self._temperature),
            "out    : en=%d  Iref=%.3fA  Vref=%.3fV  norm=%.4f"
            % (1 if self.out_enabled else 0, self.out_current, self.out_voltage, self.out_norm),
            "fan    : en=%d  speed=%.0f%%  target=%.0f%%"
            % (
                1 if self._fan_enabled else 0,
                (self._fan_target if self._fan_enabled else 0.0) * 100.0,
                self._fan_target * 100.0,
            ),
            "limits : OCP=%.2fA  OTP=%.0fC  Imax=%.2fA"
            % (config.OVERCURRENT_LIMIT, config.OVERTEMPERATURE_LIMIT, config.CURRENT_MAX),
        ]

    def _cmd_lmeas(self, args: list[str]) -> list[str]:
        return [
            "I=%.3fA V=%.3fV P=%.3fW R=%.3fohm T=%.1fC"
            % (self.out_current, self._voltage, self.power, self.resistance, self._temperature)
        ]

    def _cmd_lrun(self, args: list[str]) -> list[str]:
        if self.state == LoaderState.ERROR:
            return ["lrun: FAIL EXIT_BUSY (-6)"]
        self.state = LoaderState.RUNNING
        set_v, unit = self._active()
        return [
            "lrun: OK",
            "  state=%s mode=%s set=%.3f%s"
            % (STATE_NAMES[self.state], MODE_NAMES[self.mode], set_v, unit),
        ]

    def _cmd_lstop(self, args: list[str]) -> list[str]:
        if self.state != LoaderState.ERROR:
            self.state = LoaderState.IDLE
        return ["lstop: OK"]

    def _cmd_lclr(self, args: list[str]) -> list[str]:
        if self.state == LoaderState.ERROR:
            self.state = LoaderState.IDLE
            self.error = LoaderError.NONE
        return [
            "lclr: OK",
            "  state=%s error=%s" % (STATE_NAMES[self.state], ERROR_NAMES[self.error]),
        ]

    def _cmd_lmode(self, args: list[str]) -> list[str]:
        if not args:
            return ["mode=%s  (use: lmode cc|cv|cp|cr)" % MODE_NAMES[self.mode]]
        parsed = _parse_mode(args[0])
        if parsed is None:
            return ["lmode: bad mode '%s' (cc|cv|cp|cr)" % args[0]]
        self.mode = parsed
        set_v, unit = self._active()
        return ["lmode: OK -> %s  set=%.3f%s" % (MODE_NAMES[self.mode], set_v, unit)]

    def _cmd_lset(self, args: list[str]) -> list[str]:
        if len(args) == 1:
            ch = ascii_proto.SETPOINT_CHANNEL[self.mode]
            raw = args[0]
        elif len(args) == 2:
            if len(args[0]) != 1:
                return ["lset: channel must be i|v|p|r"]
            ch = args[0].lower()
            raw = args[1]
        else:
            set_v, unit = self._active()
            return [
                "usage: lset [i|v|p|r] <value>",
                "  i=A  v=V  p=W  r=ohm; omit channel => active mode",
                "  now mode=%s set=%.3f%s" % (MODE_NAMES[self.mode], set_v, unit),
            ]

        try:
            value = float(raw)
        except ValueError:
            return ["lset: bad number '%s'" % raw]

        if ch == "i":
            self.current_setpoint = _clamp(value, 0.0, config.CURRENT_MAX)
        elif ch == "v":
            self.voltage_setpoint = _clamp(value, 0.0, config.VOLTAGE_MAX)
        elif ch == "p":
            self.power_setpoint = _clamp(value, 0.0, config.POWER_MAX)
        elif ch == "r":
            self.resistance_setpoint = _clamp(value, config.RESISTANCE_MIN, config.RESISTANCE_MAX)
        else:
            return ["lset: channel must be i|v|p|r"]

        return [
            "lset: OK  I=%.3fA V=%.3fV P=%.3fW R=%.3fohm"
            % (
                self.current_setpoint,
                self.voltage_setpoint,
                self.power_setpoint,
                self.resistance_setpoint,
            )
        ]

    def _cmd_lout(self, args: list[str]) -> list[str]:
        return [
            "load_out: en=%d  Iref=%.3fA  Vref=%.3fV  norm=%.4f  (raw scale %u)"
            % (
                1 if self.out_enabled else 0,
                self.out_current,
                self.out_voltage,
                self.out_norm,
                config.OUT_RESOLUTION,
            )
        ]

    def _cmd_lfan(self, args: list[str]) -> list[str]:
        if not args:
            return [
                "fan: en=%d  speed=%.0f%%  target=%.0f%%"
                % (
                    1 if self._fan_enabled else 0,
                    (self._fan_target if self._fan_enabled else 0.0) * 100.0,
                    self._fan_target * 100.0,
                ),
                "usage: lfan on|off|<0..100>",
            ]
        arg = args[0]
        if arg == "on":
            self._fan_enabled = True
            if self._fan_target <= 0.0:
                self._fan_target = 0.5
            return ["lfan on: OK"]
        if arg == "off":
            self._fan_enabled = False
            return ["lfan off: OK"]
        try:
            pct = float(arg)
        except ValueError:
            return ["lfan: use on|off|<0..100>"]
        pct = _clamp(pct, 0.0, 100.0)
        self._fan_enabled = True
        self._fan_target = pct / 100.0
        return ["lfan: OK", "  speed=%.0f%%" % (self._fan_target * 100.0)]

    # -- helpers -----------------------------------------------------------

    def _active(self) -> tuple[float, str]:
        meta = config.MODE_META[self.mode]
        return self._setpoint_for(self.mode), meta["unit"]

    def _setpoint_for(self, mode: LoaderMode) -> float:
        return {
            LoaderMode.CC: self.current_setpoint,
            LoaderMode.CV: self.voltage_setpoint,
            LoaderMode.CP: self.power_setpoint,
            LoaderMode.CR: self.resistance_setpoint,
        }[mode]


def _clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


def _parse_mode(text: str) -> LoaderMode | None:
    t = text.lower()
    if len(t) != 2:
        return None
    return {
        "cc": LoaderMode.CC,
        "cv": LoaderMode.CV,
        "cp": LoaderMode.CP,
        "cr": LoaderMode.CR,
    }.get(t)


def _mode_from_int(value: int) -> LoaderMode | None:
    try:
        return LoaderMode(value)
    except ValueError:
        return None


class SimulatorTransport:
    """An in-process fake device speaking both protocols.

    Reproduces real console quirks: every byte is echoed, the prompt has no
    trailing newline, and a banner is emitted on open.
    """

    def __init__(self, tick_hz: float = 50.0) -> None:
        from .demux import Demux

        self.device = SimulatedDevice()
        self._out: deque[int] = deque()
        self._lock = threading.Lock()
        self._line_buf = ""
        self._open = False
        self._last_tick = time.monotonic()
        self._tick_period = 1.0 / tick_hz
        # Host -> device direction is demuxed too, so binary frames are
        # recognised instead of being fed to the line parser as garbage.
        self._rx_demux = Demux(frame_timeout_s=0.05)
        self.binary_enabled = True
        self.received_frames: list[tuple[int, int, bytes]] = []

    # -- interface ---------------------------------------------------------

    def open(self) -> None:
        self._open = True
        self._emit(_BANNER)
        self._prompt()
        self._last_tick = time.monotonic()

    def close(self) -> None:
        self._open = False

    @property
    def is_open(self) -> bool:
        return self._open

    @property
    def description(self) -> str:
        return "simulator (内置模拟器)"

    def write(self, data: bytes) -> None:
        """Host -> device.

        Bytes are echoed (real console behaviour), then demuxed: any complete
        binary frame is executed by the binary handler, and whatever ASCII
        remains is fed to the line-oriented shell.
        """
        if not self._open:
            return
        for byte in data:
            self._echo_byte(byte)

        ascii_bytes, frames = self._rx_demux.feed(data)
        for frame in frames:
            self._handle_binary_frame(frame)

        if not ascii_bytes:
            return
        with self._lock:
            self._line_buf += ascii_bytes.decode("ascii", errors="replace")

        # The shell executes on newline; LF or CRLF both terminate.
        while "\n" in self._line_buf:
            line, self._line_buf = self._line_buf.split("\n", 1)
            line = line.rstrip("\r")
            self._execute(line)

    def read(self, max_bytes: int = 4096, timeout_s: float | None = None) -> bytes:
        if not self._open:
            return b""
        self._advance()
        self._rx_demux.tick()  # abort a half-sent host frame after a byte gap
        with self._lock:
            if not self._out:
                return b""
            take = min(max_bytes, len(self._out))
            return bytes(self._out.popleft() for _ in range(take))

    # -- binary protocol ---------------------------------------------------

    def _handle_binary_frame(self, frame) -> None:
        """Service one host request frame with a binary response."""
        from . import binary_proto as bp

        self.received_frames.append((frame.seq, frame.cmd, frame.payload))
        if not self.binary_enabled:
            return

        opcode = frame.opcode
        seq = frame.seq

        try:
            if opcode == bp.Cmd.PING:
                self._emit_bytes(bp.make_response(seq, bp.Cmd.PING))
            elif opcode in (bp.Cmd.GET_STATUS, bp.Cmd.GET_OUT):
                payload = bp.pack_status(self.device.snapshot())
                self._emit_bytes(bp.make_response(seq, bp.Cmd(opcode), payload))
            elif opcode == bp.Cmd.SET_SETPOINT:
                channel, value = bp.unpack_setpoint(frame.payload)
                self._apply_setpoint(channel, value)
                self._emit_bytes(bp.make_response(seq, bp.Cmd.SET_SETPOINT))
            elif opcode == bp.Cmd.SET_MODE:
                parsed = _mode_from_int(bp.unpack_mode(frame.payload))
                if parsed is None:
                    self._emit_bytes(bp.make_error(seq, -3))  # EXIT_INVALID_PARAM
                else:
                    self.device.mode = parsed
                    self._emit_bytes(bp.make_response(seq, bp.Cmd.SET_MODE))
            elif opcode == bp.Cmd.RUN:
                lines = self.device.handle_command("lrun")
                busy = bool(lines) and "FAIL" in lines[0]
                if busy:
                    self._emit_bytes(bp.make_error(seq, -6))  # EXIT_BUSY
                else:
                    self._emit_bytes(bp.make_response(seq, bp.Cmd.RUN))
            elif opcode == bp.Cmd.STOP:
                self.device.handle_command("lstop")
                self._emit_bytes(bp.make_response(seq, bp.Cmd.STOP))
            elif opcode == bp.Cmd.CLEAR_FAULT:
                self.device.handle_command("lclr")
                self._emit_bytes(bp.make_response(seq, bp.Cmd.CLEAR_FAULT))
            elif opcode == bp.Cmd.SET_FAN:
                enabled, percent = bp.unpack_fan(frame.payload)
                self.device._fan_enabled = enabled
                self.device._fan_target = _clamp(percent / 100.0, 0.0, 1.0)
                self._emit_bytes(bp.make_response(seq, bp.Cmd.SET_FAN))
            else:
                self._emit_bytes(bp.make_error(seq, -4))  # EXIT_NOT_SUPPORTED
        except Exception:
            self._emit_bytes(bp.make_error(seq, -1))  # EXIT_FAIL

    def _apply_setpoint(self, channel: int, value: float) -> None:
        ch = chr(channel) if channel else ascii_proto.SETPOINT_CHANNEL[self.device.mode]
        d = self.device
        if ch == "i":
            d.current_setpoint = _clamp(value, 0.0, config.CURRENT_MAX)
        elif ch == "v":
            d.voltage_setpoint = _clamp(value, 0.0, config.VOLTAGE_MAX)
        elif ch == "p":
            d.power_setpoint = _clamp(value, 0.0, config.POWER_MAX)
        elif ch == "r":
            d.resistance_setpoint = _clamp(value, config.RESISTANCE_MIN, config.RESISTANCE_MAX)

    def push_telemetry(self, seq: int = 0) -> None:
        """Emit an unsolicited telemetry frame, as a firmware would."""
        from . import binary_proto as bp

        if self._open and self.binary_enabled:
            self._emit_bytes(bp.make_telemetry(seq, self.device.snapshot()))

    def _emit_bytes(self, data: bytes) -> None:
        with self._lock:
            self._out.extend(data)

    # -- internals ---------------------------------------------------------

    def _advance(self) -> None:
        """Run the physics model at a fixed rate, independent of poll rate."""
        now = time.monotonic()
        elapsed = now - self._last_tick
        self._last_tick = now
        # Clamp so a long stall does not explode the integrator.
        self.device.step(min(elapsed, 0.2))

    def _execute(self, line: str) -> None:
        if not line.strip():
            self._prompt()
            return
        for reply in self.device.handle_command(line):
            self._emit(reply + "\r\n")
        self._prompt()

    def _echo_byte(self, byte: int) -> None:
        # The shell echoes every byte, and renders a bare CR as CRLF.
        if byte == 0x0A:
            self._emit("\r\n")
        elif byte == 0x0D:
            pass  # CRLF input: the LF already emitted the newline
        elif 0x20 <= byte < 0x7F:
            self._emit(chr(byte))
        # Other control bytes produce no visible echo, matching the shell.

    def _prompt(self) -> None:
        self._emit(_PROMPT)

    def _emit(self, text: str) -> None:
        with self._lock:
            self._out.extend(text.encode("utf-8", errors="replace"))


__all__ = [
    "SerialTransport",
    "SimulatedDevice",
    "SimulatorTransport",
    "TransportError",
    "list_serial_ports",
]
