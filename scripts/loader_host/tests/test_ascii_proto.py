"""Tests for ASCII command building and tolerant reply parsing.

The expected strings are copied verbatim from the print formats in
`src/app/loader_cli.c`, including the exact spacing and unit suffixes, so a
firmware format change shows up here.
"""

from __future__ import annotations

from loader_host.ascii_proto import (
    AsciiParser,
    build_command,
    cmd_clear_fault,
    cmd_fan_off,
    cmd_fan_on,
    cmd_fan_percent,
    cmd_meas,
    cmd_mode,
    cmd_run,
    cmd_set,
    cmd_status,
    cmd_stop,
    strip_noise,
)
from loader_host.config import LoaderError, LoaderMode, LoaderState


def _parse(*lines: str) -> list:
    parser = AsciiParser()
    events = []
    for line in lines:
        events.extend(parser.feed((line + "\r\n").encode()))
    return events


def _snapshots(events) -> list:
    return [e.snapshot for e in events if e.kind == "snapshot" and e.snapshot is not None]


def _results(events) -> list:
    return [e.result for e in events if e.kind == "result" and e.result is not None]


# ---------------------------------------------------------------------------
# Command construction
# ---------------------------------------------------------------------------


def test_build_command_uses_lf_terminator():
    """SHELL_ENTER_LF is enabled and lone CR is not, so we must end with LF."""
    assert build_command("lmeas") == b"lmeas\n"


def test_build_command_with_args():
    assert build_command("lset", "i", f"{1.5:.3f}") == b"lset i 1.500\n"


def test_command_helpers():
    assert cmd_meas() == b"lmeas\n"
    assert cmd_status() == b"lstatus\n"
    assert cmd_run() == b"lrun\n"
    assert cmd_stop() == b"lstop\n"
    assert cmd_clear_fault() == b"lclr\n"
    assert cmd_fan_on() == b"lfan on\n"
    assert cmd_fan_off() == b"lfan off\n"
    assert cmd_fan_percent(75.0) == b"lfan 75\n"
    assert cmd_mode(LoaderMode.CV) == b"lmode cv\n"


def test_cmd_set_by_channel_and_by_active_mode():
    assert cmd_set(2.5, "i") == b"lset i 2.500\n"
    assert cmd_set(2.5) == b"lset 2.500\n"
    assert cmd_set(1000.0, "r", decimals=1) == b"lset r 1000.0\n"


# ---------------------------------------------------------------------------
# Noise stripping
# ---------------------------------------------------------------------------


def test_strip_ansi_colour_codes():
    assert strip_noise("\x1b[32minfo\x1b[0m state  : IDLE") == "info state  : IDLE"


def test_strip_prompt_token():
    assert strip_noise("letter:/$ state  : IDLE") == "state  : IDLE"


def test_strip_prompt_glued_to_line():
    """The prompt has no newline, so it is prefixed onto the next reply."""
    assert strip_noise("letter:/$ I=0.000A V=0.000V P=0.000W R=0.000ohm T=0.0C") == (
        "I=0.000A V=0.000V P=0.000W R=0.000ohm T=0.0C"
    )


def test_echo_of_our_own_command_is_not_a_snapshot():
    """The shell echoes input; that must not be mistaken for data."""
    events = _parse("lmeas")
    assert _snapshots(events) == []


# ---------------------------------------------------------------------------
# lmeas
# ---------------------------------------------------------------------------


def test_lmeas_bare_line():
    events = _parse("I=1.234A V=12.345V P=15.234W R=10.004ohm T=31.5C")
    snaps = _snapshots(events)
    assert len(snaps) == 1
    snap = snaps[0]
    assert snap.current_measurement == 1.234
    assert snap.voltage_measurement == 12.345
    assert snap.power_measurement == 15.234
    assert snap.resistance_measurement == 10.004
    assert snap.temperature_measurement == 31.5


def test_lmeas_with_leading_echo_and_prompt():
    """Real capture shape: our echo, then the prompt+reply on one line."""
    events = _parse(
        "lmeas",
        "letter:/$ I=0.000A V=0.000V P=0.000W R=0.000ohm T=25.0C",
        "letter:/$ ",
    )
    snaps = _snapshots(events)
    assert len(snaps) == 1
    assert snaps[0].temperature_measurement == 25.0


# ---------------------------------------------------------------------------
# lstatus
# ---------------------------------------------------------------------------

STATUS_REPLY = [
    "--- loader status ---",
    "state  : RUNNING",
    "error  : NONE",
    "mode   : CC",
    "set    : I=1.500A  V=0.000V  P=0.000W  R=1000.000ohm",
    "active : 1.500 A",
    "meas   : I=1.499A  V=11.980V  P=17.958W  R=7.992ohm  T=33.2C",
    "out    : en=1  Iref=1.500A  Vref=0.750V  norm=0.2273",
    "fan    : en=1  speed=50%  target=50%",
    "limits : OCP=5.00A  OTP=50C  Imax=5.00A",
]


def test_lstatus_full_parse():
    snaps = _snapshots(_parse(*STATUS_REPLY))
    merged = snaps[0]
    for snap in snaps[1:]:
        merged = merged.merged_with(snap)

    assert merged.state == LoaderState.RUNNING
    assert merged.error == LoaderError.NONE
    assert merged.mode == LoaderMode.CC
    assert merged.current_setpoint == 1.5
    assert merged.resistance_setpoint == 1000.0
    assert merged.current_measurement == 1.499
    assert merged.temperature_measurement == 33.2
    assert merged.out_enabled is True
    assert merged.out_norm == 0.2273
    assert merged.fan_enabled is True
    assert merged.fan_speed == 0.5
    assert merged.fan_target == 0.5
    assert merged.ocp_limit == 5.0
    assert merged.otp_limit == 50.0
    assert merged.current_max == 5.0


def test_active_setpoint_property_follows_mode():
    snaps = _snapshots(_parse(*STATUS_REPLY))
    merged = snaps[0]
    for snap in snaps[1:]:
        merged = merged.merged_with(snap)
    assert merged.active_setpoint == 1.5  # CC -> current


def test_lstatus_faulted_state():
    snaps = _snapshots(_parse("state  : ERROR", "error  : OCP"))
    merged = snaps[0].merged_with(snaps[1])
    assert merged.state == LoaderState.ERROR
    assert merged.error == LoaderError.OVERCURRENT
    assert merged.is_faulted


# ---------------------------------------------------------------------------
# Results
# ---------------------------------------------------------------------------


def test_lrun_ok_and_continuation():
    snaps = _snapshots(_parse("lrun: OK", "  state=RUNNING mode=CC set=1.500A"))
    results = _results(_parse("lrun: OK", "  state=RUNNING mode=CC set=1.500A"))
    assert len(results) == 1
    assert results[0].ok
    assert results[0].command == "lrun"

    merged = snaps[0]
    for snap in snaps[1:]:
        merged = merged.merged_with(snap)
    assert merged.state == LoaderState.RUNNING
    assert merged.mode == LoaderMode.CC
    assert merged.current_setpoint == 1.5


def test_lrun_rejected_busy_reports_exit_code():
    results = _results(_parse("lrun: FAIL EXIT_BUSY (-6)"))
    assert len(results) == 1
    assert not results[0].ok
    assert results[0].exit_code == -6


def test_lstop_and_lclr_ok():
    results = _results(_parse("lstop: OK", "lclr: OK", "  state=IDLE error=NONE"))
    assert [r.command for r in results] == ["lstop", "lclr"]
    assert all(r.ok for r in results)


def test_lmode_ok_continuation():
    results = _results(_parse("lmode: OK -> CV  set=12.000V"))
    assert len(results) == 1
    assert results[0].ok
    assert results[0].command == "lmode"


def test_lmode_bad_mode_is_failure():
    results = _results(_parse("lmode: bad mode 'xx' (cc|cv|cp|cr)"))
    assert len(results) == 1
    assert not results[0].ok


def test_lmode_query_reports_mode():
    snaps = _snapshots(_parse("mode=CR  (use: lmode cc|cv|cp|cr)"))
    assert snaps[0].mode == LoaderMode.CR


def test_lset_ok_echoes_clamped_values():
    """The firmware clamps silently, so the echo is the source of truth."""
    snaps = _snapshots(_parse("lset: OK  I=5.000A V=0.000V P=0.000W R=1000.000ohm"))
    results = _results(_parse("lset: OK  I=5.000A V=0.000V P=0.000W R=1000.000ohm"))
    assert results[0].ok
    assert snaps[0].current_setpoint == 5.0  # clamped from an oversized request


def test_lset_bad_number_is_failure():
    results = _results(_parse("lset: bad number 'abc'"))
    assert len(results) == 1
    assert not results[0].ok


def test_lset_usage_lines_are_text_not_failures():
    parser = AsciiParser()
    events = parser.feed(b"usage: lset [i|v|p|r] <value>\r\n")
    events += parser.feed(b"  i=A  v=V  p=W  r=ohm; omit channel => active mode\r\n")
    assert [e.kind for e in events] == ["text", "text"]


# ---------------------------------------------------------------------------
# lout / lfan
# ---------------------------------------------------------------------------


def test_lout_bare_line():
    events = _parse("load_out: en=1  Iref=1.500A  Vref=0.750V  norm=0.2273  (raw scale 4095)")
    snaps = _snapshots(events)
    assert snaps[0].out_enabled is True
    assert snaps[0].out_current == 1.5
    assert snaps[0].out_voltage == 0.75
    assert snaps[0].out_norm == 0.2273


def test_lout_labelled_line_inside_status():
    snaps = _snapshots(_parse("out    : en=0  Iref=0.000A  Vref=0.000V  norm=0.0000"))
    assert snaps[0].out_enabled is False
    assert snaps[0].out_norm == 0.0


def test_lfan_query_line():
    snaps = _snapshots(_parse("fan: en=1  speed=75%  target=80%"))
    assert snaps[0].fan_enabled is True
    assert snaps[0].fan_speed == 0.75
    assert snaps[0].fan_target == 0.80


def test_lfan_percent_result_and_speed_echo():
    results = _results(_parse("lfan: OK", "  speed=60%"))
    assert results[0].ok
    snaps = _snapshots(_parse("lfan: OK", "  speed=60%"))
    # Only the `speed=` line carries data, so exactly one snapshot is emitted.
    assert len(snaps) == 1
    assert snaps[0].fan_speed == 0.6


def test_lfan_on_and_off_results():
    results = _results(_parse("lfan on: OK", "lfan off: OK"))
    assert [r.command for r in results] == ["lfan on", "lfan off"]


# ---------------------------------------------------------------------------
# Robustness
# ---------------------------------------------------------------------------


def test_banner_and_unknown_lines_become_text():
    events = _parse("BaseFramework H750", "letter:/$ ", "some random text")
    assert all(e.kind == "text" for e in events)


def test_partial_line_is_buffered_until_complete():
    parser = AsciiParser()
    assert parser.feed(b"I=1.000A V=2.0") == []
    events = parser.feed(b"00V P=2.000W R=2.000ohm T=25.0C\r\n")
    snaps = _snapshots(events)
    assert len(snaps) == 1
    assert snaps[0].voltage_measurement == 2.0


def test_line_split_across_chunks_with_crlf_boundary():
    parser = AsciiParser()
    parser.feed(b"state  : RUN")
    events = parser.feed(b"NING\r\n")
    snaps = _snapshots(events)
    assert snaps[0].state == LoaderState.RUNNING


def test_flush_emits_partial_line():
    parser = AsciiParser()
    parser.feed(b"state  : IDLE")
    events = parser.flush()
    snaps = _snapshots(events)
    assert snaps[0].state == LoaderState.IDLE


def test_negative_and_exponent_numbers_parse():
    snaps = _snapshots(_parse("I=-0.001A V=1.5e+01V P=0.000W R=0.000ohm T=-5.0C"))
    snap = snaps[0]
    assert snap.current_measurement == -0.001
    assert snap.voltage_measurement == 15.0
    assert snap.temperature_measurement == -5.0
