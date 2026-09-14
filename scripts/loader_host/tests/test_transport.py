"""Tests for the simulator transport (device model + echo/prompt quirks)."""

from __future__ import annotations

import time

import pytest

from loader_host.ascii_proto import AsciiParser
from loader_host.binary_proto import Cmd, make_request
from loader_host.config import LoaderMode, LoaderState
from loader_host.demux import Demux
from loader_host.transport import SimulatedDevice, SimulatorTransport

# ---------------------------------------------------------------------------
# Device model
# ---------------------------------------------------------------------------


def test_device_starts_idle_and_disabled():
    d = SimulatedDevice()
    assert d.state == LoaderState.IDLE
    assert not d.out_enabled
    assert d.current_setpoint == 0.0
    assert d.resistance_setpoint == 1000.0


def test_run_requires_no_fault():
    d = SimulatedDevice()
    lines = d.handle_command("lrun")
    assert lines[0] == "lrun: OK"
    assert d.state == LoaderState.RUNNING
    assert d.out_enabled


def test_run_from_fault_is_busy():
    from loader_host.config import LoaderError

    d = SimulatedDevice()
    d._fault(LoaderError.OVERCURRENT)
    lines = d.handle_command("lrun")
    assert lines[0] == "lrun: FAIL EXIT_BUSY (-6)"


def test_clear_fault_returns_to_idle():
    d = SimulatedDevice()
    d.state = LoaderState.ERROR
    lines = d.handle_command("lclr")
    assert lines[0] == "lclr: OK"
    assert d.state == LoaderState.IDLE


def test_setpoint_is_silently_clamped():
    d = SimulatedDevice()
    lines = d.handle_command("lset i 99")
    assert lines[0].startswith("lset: OK")
    assert "I=5.000A" in lines[0]


def test_mode_switch_is_case_insensitive():
    d = SimulatedDevice()
    assert d.handle_command("lmode CV")[0].startswith("lmode: OK")
    assert d.mode == LoaderMode.CV


def test_bad_mode_rejected():
    d = SimulatedDevice()
    assert "bad mode" in d.handle_command("lmode zz")[0]


def test_fan_percent_clamped():
    d = SimulatedDevice()
    d.handle_command("lfan 500")
    assert d._fan_target == 1.0
    assert d._fan_enabled


def test_cc_mode_converges_towards_setpoint():
    d = SimulatedDevice()
    d.handle_command("lmode cc")
    d.handle_command("lset i 2.0")
    d.handle_command("lrun")
    for _ in range(200):
        d.step(0.02)
    assert d._current == pytest.approx(2.0, abs=0.05)
    assert d._voltage < 12.0  # droops under load


def test_thermal_protection_eventually_trips():
    """Sustained high current with no fan must latch OTP."""
    d = SimulatedDevice()
    d.handle_command("lmode cc")
    d.handle_command("lset i 4.5")
    d.handle_command("lrun")
    for _ in range(2000):
        d.step(0.02)
        if d.state == LoaderState.ERROR:
            break
    assert d.state == LoaderState.ERROR


# ---------------------------------------------------------------------------
# Transport: echo and prompt fidelity
# ---------------------------------------------------------------------------


def _drain(t: SimulatorTransport) -> bytes:
    return t.read(65536)


def test_open_emits_banner_and_prompt():
    t = SimulatorTransport()
    t.open()
    out = _drain(t)
    assert b"BaseFramework H750" in out
    assert b"letter:/$ " in out


def test_write_echoes_input():
    t = SimulatorTransport()
    t.open()
    _drain(t)
    t.write(b"lmeas\n")
    out = _drain(t)
    assert b"lmeas" in out  # the echo
    assert b"I=" in out  # the reply


def test_parser_ignores_echo_prompt_and_banner():
    """End-to-end: the parser must extract data from the noisy stream."""
    t = SimulatorTransport()
    t.open()
    parser = AsciiParser()
    parser.feed(_drain(t))

    t.write(b"lmeas\n")
    events = parser.feed(_drain(t))
    snaps = [e.snapshot for e in events if e.kind == "snapshot"]
    assert len(snaps) == 1
    assert snaps[0].temperature_measurement is not None


def test_status_roundtrip_through_simulator():
    t = SimulatorTransport()
    t.open()
    parser = AsciiParser()
    parser.feed(_drain(t))

    t.write(b"lmode cc\n")
    t.write(b"lset i 1.5\n")
    t.write(b"lrun\n")
    t.write(b"lstatus\n")

    events = parser.feed(_drain(t))
    merged = None
    for e in events:
        if e.kind == "snapshot" and e.snapshot is not None:
            merged = e.snapshot if merged is None else merged.merged_with(e.snapshot)

    assert merged is not None
    assert merged.state == LoaderState.RUNNING
    assert merged.mode == LoaderMode.CC
    assert merged.current_setpoint == pytest.approx(1.5)


def test_simulator_responds_to_binary_request():
    t = SimulatorTransport()
    t.open()
    _drain(t)

    t.write(make_request(seq=42, cmd=Cmd.PING))
    demux = Demux()
    responses = []
    deadline = time.monotonic() + 1.0
    while time.monotonic() < deadline and not responses:
        _, frames = demux.feed(_drain(t))
        responses.extend(frames)
        time.sleep(0.01)

    assert len(responses) == 1
    assert responses[0].seq == 42
    assert responses[0].opcode == int(Cmd.PING)
    assert t.received_frames[0][0] == 42


def test_simulator_binary_get_status_returns_valid_frame():
    t = SimulatorTransport()
    t.open()
    parser = AsciiParser()  # consumes echo; not used for assertions
    parser.feed(_drain(t))

    t.write(b"lmode cc\n")
    t.write(b"lset i 1.0\n")
    t.write(b"lrun\n")
    _drain(t)

    from loader_host.binary_proto import unpack_status

    t.write(make_request(seq=7, cmd=Cmd.GET_STATUS))
    demux = Demux()
    frames = []
    deadline = time.monotonic() + 1.0
    while time.monotonic() < deadline and not frames:
        _, reply = demux.feed(_drain(t))
        frames.extend(reply)
        time.sleep(0.01)

    assert len(frames) == 1
    snap = unpack_status(frames[0].payload)
    assert snap.state == LoaderState.RUNNING
    assert snap.current_setpoint == pytest.approx(1.0)


def test_simulator_binary_error_for_unknown_opcode():
    t = SimulatorTransport()
    t.open()
    _drain(t)
    t.write(make_request(seq=3, cmd=0x7E))  # not a valid request opcode

    demux = Demux()
    frames = []
    deadline = time.monotonic() + 1.0
    while time.monotonic() < deadline and not frames:
        _, reply = demux.feed(_drain(t))
        frames.extend(reply)
        time.sleep(0.01)

    assert len(frames) == 1
    assert frames[0].cmd == 0xFF
    from loader_host.binary_proto import unpack_error

    assert unpack_error(frames[0].payload) == -4  # EXIT_NOT_SUPPORTED
