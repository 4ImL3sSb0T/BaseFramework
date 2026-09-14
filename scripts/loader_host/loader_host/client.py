"""Host client: background worker + request/response scheduling.

Threading model
---------------
Qt's main thread owns the GUI. All serial I/O happens in a `QThread` running
`HostWorker`. The two communicate only through:

* a `queue.Queue` of outgoing commands (main -> worker), and
* Qt signals (worker -> main).

No mutable state is shared, which keeps the GUI responsive even when the
device is silent (a common case: nothing is plugged in).

Scheduling
----------
115200 baud is ~11.5 kB/s, so reply budget matters:

* `lmeas`  (one ~60-byte line) drives the live curves, default 10 Hz;
* `lstatus` (nine lines, ~400 bytes) refreshes the slower fields, default 1 Hz.

Only one request may be in flight at a time, otherwise replies interleave and
the parser can mis-attribute values across commands.
"""

from __future__ import annotations

import queue
import time
from collections import Counter
from dataclasses import dataclass

from PySide6.QtCore import QObject, QThread, Signal

from .ascii_proto import (
    AsciiParser,
    cmd_clear_fault,
    cmd_fan_off,
    cmd_fan_on,
    cmd_fan_percent,
    cmd_meas,
    cmd_mode,
    cmd_out,
    cmd_run,
    cmd_set,
    cmd_status,
    cmd_stop,
)
from .demux import Demux
from .model import LoaderSnapshot
from .transport import SerialTransport, SimulatorTransport, TransportError, list_serial_ports


@dataclass
class PollConfig:
    meas_hz: float = 10.0
    status_hz: float = 1.0
    binary_hz: float = 0.0  # 0 disables the binary status poll

    @property
    def meas_period(self) -> float:
        return 1.0 / self.meas_hz if self.meas_hz > 0 else 0.0

    @property
    def status_period(self) -> float:
        return 1.0 / self.status_hz if self.status_hz > 0 else 0.0

    @property
    def binary_period(self) -> float:
        return 1.0 / self.binary_hz if self.binary_hz > 0 else 0.0


@dataclass
class LinkStats:
    tx_bytes: int = 0
    rx_bytes: int = 0
    poll_count: int = 0
    parse_hits: int = 0
    frames_ok: int = 0
    crc_errors: int = 0
    resyncs: int = 0
    last_rx: float = 0.0


class HostWorker(QObject):
    """Runs the serial link in a background thread."""

    snapshot_updated = Signal(object)  # LoaderSnapshot (accumulated)
    partial_updated = Signal(object)  # LoaderSnapshot (this reply only)
    line_received = Signal(str)  # raw-ish text for the terminal panel
    result_received = Signal(object)  # CommandResult
    connection_changed = Signal(bool, str)  # connected, description
    error_occurred = Signal(str)
    stats_updated = Signal(object)  # LinkStats
    frame_received = Signal(object)  # binary Frame

    def __init__(self) -> None:
        super().__init__()
        self._commands: queue.Queue[str | tuple] = queue.Queue()
        self._transport = None
        self._demux = Demux()
        self._parser = AsciiParser()
        self._acc = LoaderSnapshot()
        self._poll = PollConfig()
        self._stats = LinkStats()
        self._stop = False
        self._binary_enabled = False
        self._seq = 0
        self._pending_binary: dict[int, tuple[float, object]] = {}
        # The shell echoes every byte we send, which at 10 Hz polling would
        # flood the terminal panel. Track what we sent so the echo can be
        # recognised and dropped instead of displayed.
        self._pending_echoes: Counter[str] = Counter()

    # -- control (called from the GUI thread) ------------------------------

    def request_connect(self, port: str | None, baudrate: int) -> None:
        self._commands.put(("_connect", port, baudrate))

    def request_disconnect(self) -> None:
        self._commands.put(("_disconnect",))

    def request_quit(self) -> None:
        self._stop = True
        self._commands.put(("_quit",))

    def send_line(self, text: str) -> None:
        """Send a raw console line (terminal panel)."""
        self._commands.put(("_raw", text))

    def enqueue_op(self, name: str, *args) -> None:
        self._commands.put(("_op", name, args))

    def set_poll(self, config: PollConfig) -> None:
        self._commands.put(("_poll", config))

    def set_binary_enabled(self, enabled: bool) -> None:
        self._commands.put(("_binary", enabled))

    # -- thread body -------------------------------------------------------

    def run(self) -> None:
        """Main worker loop; blocks until `request_quit` (or disconnect)."""
        next_meas = next_status = next_binary = 0.0

        while not self._stop:
            now = time.monotonic()

            # 1) Drain commands (control has priority over polling).
            try:
                while True:
                    item = self._commands.get_nowait()
                    self._handle_command(item)
                    if self._stop:
                        break
            except queue.Empty:
                pass

            if self._stop:
                break

            if self._transport is None or not self._transport.is_open:
                time.sleep(0.05)
                continue

            # 2) Read and distribute whatever arrived.
            try:
                data = self._transport.read(8192, 0.02)
            except TransportError as exc:
                self.error_occurred.emit(str(exc))
                self._disconnect("link error")
                continue

            if data:
                self._stats.rx_bytes += len(data)
                self._stats.last_rx = time.monotonic()
                self._distribute(data)
            else:
                # Idle: yield instead of spinning. Without this the loop runs
                # millions of iterations per second, burning a core and
                # starving the GUI thread of the GIL.
                time.sleep(0.005)

            self._demux.tick()

            # 3) Poll on schedule, one request in flight at a time.
            now = time.monotonic()
            if self._pending_binary:
                # Expire stale binary requests so the scheduler cannot stall.
                for seq, (sent_at, cmd) in list(self._pending_binary.items()):
                    if now - sent_at > 0.5:
                        self._pending_binary.pop(seq, None)

            if self._binary_enabled and self._poll.binary_period > 0 and now >= next_binary:
                self._send_binary_status()
                next_binary = now + self._poll.binary_period
            elif self._poll.status_period > 0 and now >= next_status:
                self._write_ascii(cmd_status())
                next_status = now + self._poll.status_period
            elif self._poll.meas_period > 0 and now >= next_meas:
                self._write_ascii(cmd_meas())
                next_meas = now + self._poll.meas_period

        self._disconnect("quit")

    # -- command handling --------------------------------------------------

    def _handle_command(self, item) -> None:
        kind = item[0]

        if kind == "_connect":
            _, port, baudrate = item
            self._connect(port, baudrate)
        elif kind == "_disconnect":
            self._disconnect("user")
        elif kind == "_quit":
            self._stop = True
        elif kind == "_raw":
            text = item[1]
            self._write_ascii(text.encode("ascii", errors="replace") + b"\n")
            self.line_received.emit(f">>> {text}")
        elif kind == "_poll":
            self._poll = item[1]
        elif kind == "_binary":
            self._binary_enabled = bool(item[1])
        elif kind == "_op":
            _, name, args = item
            self._run_op(name, args)

    def _run_op(self, name: str, args: tuple) -> None:
        """Send a control command; its reply is parsed into a CommandResult."""
        match name:
            case "run":
                self._write_ascii(cmd_run())
            case "stop":
                self._write_ascii(cmd_stop())
            case "clear_fault":
                self._write_ascii(cmd_clear_fault())
            case "meas":
                self._write_ascii(cmd_meas())
            case "status":
                self._write_ascii(cmd_status())
            case "out":
                self._write_ascii(cmd_out())
            case "mode":
                self._write_ascii(cmd_mode(args[0]))
            case "set":
                value, channel = args
                self._write_ascii(cmd_set(value, channel))
            case "fan_on":
                self._write_ascii(cmd_fan_on())
            case "fan_off":
                self._write_ascii(cmd_fan_off())
            case "fan_percent":
                self._write_ascii(cmd_fan_percent(args[0]))
            case "ping":
                self._send_binary_ping()
            case _:
                self.error_occurred.emit(f"unknown op: {name}")

    # -- link management ---------------------------------------------------

    def _connect(self, port: str | None, baudrate: int) -> None:
        self._disconnect(None)

        if port is None or port == "__sim__":
            transport = SimulatorTransport()
        else:
            transport = SerialTransport(port, baudrate)

        try:
            transport.open()
        except TransportError as exc:
            self.error_occurred.emit(f"connect failed: {exc}")
            self.connection_changed.emit(False, str(exc))
            return

        self._transport = transport
        self._demux = Demux()
        self._parser = AsciiParser()
        self._acc = LoaderSnapshot()
        self._pending_binary.clear()
        self.connection_changed.emit(True, transport.description)

    def _disconnect(self, reason: str | None) -> None:
        if self._transport is not None:
            self._transport.close()
            desc = self._transport.description
            self._transport = None
            self.connection_changed.emit(False, f"{desc} ({reason})" if reason else desc)

    def _write(self, data: bytes) -> None:
        if self._transport is None or not self._transport.is_open:
            return
        try:
            self._transport.write(data)
            self._stats.tx_bytes += len(data)
        except TransportError as exc:
            self.error_occurred.emit(str(exc))
            self._disconnect("write error")

    def _write_ascii(self, data: bytes) -> None:
        """Send an ASCII command and remember its text so the echo is dropped."""
        text = data.decode("ascii", errors="replace").strip()
        if text:
            self._pending_echoes[text] += 1
        self._write(data)

    def _is_echo(self, text: str) -> bool:
        """True if this line is our own command coming back."""
        if self._pending_echoes.get(text):
            self._pending_echoes[text] -= 1
            if self._pending_echoes[text] <= 0:
                del self._pending_echoes[text]
            return True
        return False

    # -- receive path ------------------------------------------------------

    def _distribute(self, data: bytes) -> None:
        ascii_bytes, frames = self._demux.feed(data)

        for frame in frames:
            self._stats.frames_ok += 1
            self._pending_binary.pop(frame.seq, None)
            self.frame_received.emit(frame)
            self._handle_binary_frame(frame)

        if ascii_bytes:
            events = self._parser.feed(ascii_bytes)
            for event in events:
                if event.kind == "text":
                    if not self._is_echo(event.text):
                        self.line_received.emit(event.text)
                elif event.kind == "snapshot" and event.snapshot is not None:
                    self._stats.parse_hits += 1
                    self._acc = self._acc.merged_with(event.snapshot)
                    self.partial_updated.emit(event.snapshot)
                    self.snapshot_updated.emit(self._acc)
                elif event.kind == "result" and event.result is not None:
                    self.result_received.emit(event.result)
                    if event.snapshot is not None:
                        self._acc = self._acc.merged_with(event.snapshot)
                        self.snapshot_updated.emit(self._acc)

        self._stats.parse_hits = self._parser.lines_matched
        self._sync_demux_stats()
        self.stats_updated.emit(self._stats)

    def _sync_demux_stats(self) -> None:
        self._stats.crc_errors = self._demux.stats.crc_errors
        self._stats.resyncs = self._demux.stats.resyncs

    def _handle_binary_frame(self, frame) -> None:
        from . import binary_proto as bp

        if frame.opcode == bp.TELEMETRY_CMD or frame.opcode == bp.Cmd.GET_STATUS:
            try:
                snap = bp.unpack_status(frame.payload)
            except Exception:
                return
            self._acc = self._acc.merged_with(snap)
            self.snapshot_updated.emit(self._acc)

    # -- binary requests ---------------------------------------------------

    def _next_seq(self) -> int:
        self._seq = (self._seq + 1) & 0xFF
        return self._seq

    def _send_binary_status(self) -> None:
        from . import binary_proto as bp

        seq = self._next_seq()
        self._pending_binary[seq] = (time.monotonic(), bp.Cmd.GET_STATUS)
        self._write(bp.make_request(seq, bp.Cmd.GET_STATUS))

    def _send_binary_ping(self) -> None:
        from . import binary_proto as bp

        seq = self._next_seq()
        self._pending_binary[seq] = (time.monotonic(), bp.Cmd.PING)
        self._write(bp.make_request(seq, bp.Cmd.PING))


class HostClient(QObject):
    """GUI-facing facade: owns the thread and re-exposes its signals.

    Keeping the thread plumbing here means the window code never touches
    QThread directly.
    """

    snapshot_updated = Signal(object)
    partial_updated = Signal(object)
    line_received = Signal(str)
    result_received = Signal(object)
    connection_changed = Signal(bool, str)
    error_occurred = Signal(str)
    stats_updated = Signal(object)
    frame_received = Signal(object)

    def __init__(self, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._thread = QThread()
        self._worker = HostWorker()
        self._worker.moveToThread(self._thread)

        self._worker.snapshot_updated.connect(self.snapshot_updated)
        self._worker.partial_updated.connect(self.partial_updated)
        self._worker.line_received.connect(self.line_received)
        self._worker.result_received.connect(self.result_received)
        self._worker.connection_changed.connect(self.connection_changed)
        self._worker.error_occurred.connect(self.error_occurred)
        self._worker.stats_updated.connect(self.stats_updated)
        self._worker.frame_received.connect(self.frame_received)

        self._thread.started.connect(self._worker.run)
        self._thread.start()

    # -- GUI-thread API ----------------------------------------------------

    def connect_port(self, port: str | None, baudrate: int) -> None:
        self._worker.request_connect(port, baudrate)

    def disconnect_port(self) -> None:
        self._worker.request_disconnect()

    def send_line(self, text: str) -> None:
        self._worker.send_line(text)

    def op(self, name: str, *args) -> None:
        self._worker.enqueue_op(name, *args)

    def set_poll(self, config: PollConfig) -> None:
        self._worker.set_poll(config)

    def set_binary_enabled(self, enabled: bool) -> None:
        self._worker.set_binary_enabled(enabled)

    def shutdown(self) -> None:
        self._worker.request_quit()
        self._thread.quit()
        self._thread.wait(2000)


__all__ = ["HostClient", "HostWorker", "LinkStats", "PollConfig", "list_serial_ports"]
