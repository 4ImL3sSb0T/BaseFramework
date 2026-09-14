"""Tests for the binary frame codec."""

from __future__ import annotations

import struct

import pytest

from loader_host.binary_proto import (
    CH_CURRENT,
    MAGIC0,
    MAGIC1,
    STATUS_SIZE,
    Cmd,
    Frame,
    FrameError,
    crc16_ccitt_false,
    decode_frame,
    make_error,
    make_request,
    make_response,
    make_telemetry,
    pack_error,
    pack_fan,
    pack_setpoint,
    pack_status,
    unpack_error,
    unpack_fan,
    unpack_setpoint,
    unpack_status,
)
from loader_host.config import LoaderError, LoaderMode, LoaderState
from loader_host.model import LoaderSnapshot

# ---------------------------------------------------------------------------
# CRC
# ---------------------------------------------------------------------------


def test_crc_ccitt_false_check_value():
    """The canonical CRC-16/CCITT-FALSE check value is 0x29B1."""
    assert crc16_ccitt_false(b"123456789") == 0x29B1


def test_crc_empty_is_init():
    assert crc16_ccitt_false(b"") == 0xFFFF


def test_crc_is_stable_and_byte_order_sensitive():
    assert crc16_ccitt_false(b"\x01\x02") == crc16_ccitt_false(b"\x01\x02")
    assert crc16_ccitt_false(b"\x01\x02") != crc16_ccitt_false(b"\x02\x01")


# ---------------------------------------------------------------------------
# Frame round-trip
# ---------------------------------------------------------------------------


def test_frame_roundtrip_empty_payload():
    raw = make_request(seq=7, cmd=Cmd.PING)
    frame = decode_frame(raw)
    assert frame.seq == 7
    assert frame.cmd == int(Cmd.PING)
    assert frame.payload == b""
    assert not frame.is_response


def test_frame_roundtrip_with_payload():
    payload = bytes(range(40))
    raw = Frame(seq=0x12, cmd=0x10, payload=payload).encode()
    frame = decode_frame(raw)
    assert frame.seq == 0x12
    assert frame.payload == payload
    assert len(raw) == 5 + len(payload) + 2


def test_frame_layout_is_as_specified():
    """Lock the documented byte offsets."""
    raw = Frame(seq=0xAB, cmd=0x10, payload=b"\x11\x22").encode()
    assert raw[0] == MAGIC0
    assert raw[1] == MAGIC1
    assert raw[2] == 0xAB  # seq
    assert raw[3] == 0x10  # cmd
    assert raw[4] == 2  # len
    assert raw[5:7] == b"\x11\x22"
    crc = struct.unpack_from("<H", raw, 7)[0]
    assert crc == crc16_ccitt_false(raw[2:7])


def test_response_flag_and_opcode():
    raw = make_response(seq=1, cmd=Cmd.GET_STATUS, payload=b"x")
    frame = decode_frame(raw)
    assert frame.is_response
    assert frame.opcode == int(Cmd.GET_STATUS)


def test_error_frame_carries_exit_code():
    raw = make_error(seq=3, exit_code=-6)  # EXIT_BUSY
    frame = decode_frame(raw)
    assert frame.cmd == 0xFF
    assert unpack_error(frame.payload) == -6


# ---------------------------------------------------------------------------
# Decode failures
# ---------------------------------------------------------------------------


def test_bad_magic_rejected():
    raw = bytearray(Frame(seq=1, cmd=1).encode())
    raw[0] = 0x00
    with pytest.raises(FrameError, match="bad magic"):
        decode_frame(bytes(raw))


def test_crc_error_detected():
    raw = bytearray(Frame(seq=1, cmd=1, payload=b"hello").encode())
    raw[-1] ^= 0xFF
    with pytest.raises(FrameError, match="crc mismatch"):
        decode_frame(bytes(raw))


def test_truncated_frame_rejected():
    raw = Frame(seq=1, cmd=1, payload=b"hello").encode()
    with pytest.raises(FrameError):
        decode_frame(raw[:-1])


def test_too_short_rejected():
    with pytest.raises(FrameError, match="too short"):
        decode_frame(b"\xa5\x5a\x01")


def test_oversized_payload_rejected_at_encode():
    with pytest.raises(FrameError, match="payload too long"):
        Frame(seq=1, cmd=1, payload=b"\x00" * 193).encode()


# ---------------------------------------------------------------------------
# Status payload
# ---------------------------------------------------------------------------


def _sample_snapshot() -> LoaderSnapshot:
    return LoaderSnapshot(
        state=LoaderState.RUNNING,
        error=LoaderError.NONE,
        mode=LoaderMode.CC,
        current_setpoint=1.25,
        voltage_setpoint=12.0,
        power_setpoint=15.0,
        resistance_setpoint=100.0,
        current_measurement=1.249,
        voltage_measurement=11.9,
        power_measurement=14.86,
        resistance_measurement=9.53,
        temperature_measurement=31.5,
        out_enabled=True,
        out_current=1.25,
        out_voltage=0.625,
        out_norm=0.1894,
        fan_enabled=True,
        fan_speed=0.5,
        fan_target=0.5,
    )


def test_status_size_is_64_bytes():
    assert STATUS_SIZE == 64
    assert len(pack_status(_sample_snapshot())) == 64


def test_status_roundtrip_preserves_fields():
    original = _sample_snapshot()
    restored = unpack_status(pack_status(original))

    assert restored.state == original.state
    assert restored.error == original.error
    assert restored.mode == original.mode
    assert restored.out_enabled is True
    assert restored.fan_enabled is True
    for field in (
        "current_setpoint",
        "voltage_setpoint",
        "power_setpoint",
        "resistance_setpoint",
        "current_measurement",
        "voltage_measurement",
        "power_measurement",
        "resistance_measurement",
        "temperature_measurement",
        "out_current",
        "out_voltage",
        "out_norm",
        "fan_speed",
        "fan_target",
    ):
        assert getattr(restored, field) == pytest.approx(getattr(original, field))


def test_unknown_enum_values_decode_as_none():
    """A future firmware adding a state must not crash the host."""
    payload = bytearray(pack_status(_sample_snapshot()))
    payload[0] = 99  # state byte
    snap = unpack_status(bytes(payload))
    assert snap.state is None


def test_status_payload_wrong_size_rejected():
    with pytest.raises(FrameError, match="must be 64 bytes"):
        unpack_status(b"\x00" * 10)


def test_telemetry_frame_roundtrip():
    snap = _sample_snapshot()
    frame = decode_frame(make_telemetry(seq=9, snap=snap))
    assert frame.cmd == 0x98
    assert frame.seq == 9
    restored = unpack_status(frame.payload)
    assert restored.current_measurement == pytest.approx(snap.current_measurement)


# ---------------------------------------------------------------------------
# Small payload helpers
# ---------------------------------------------------------------------------


def test_setpoint_payload_roundtrip():
    payload = pack_setpoint(CH_CURRENT, 3.3)
    channel, value = unpack_setpoint(payload)
    assert channel == ord("i")
    assert value == pytest.approx(3.3)


def test_fan_payload_roundtrip():
    enabled, percent = unpack_fan(pack_fan(True, 75.0))
    assert enabled is True
    assert percent == pytest.approx(75.0)


def test_error_code_is_clamped_to_int8():
    assert unpack_error(pack_error(-200)) == -128
    assert unpack_error(pack_error(500)) == 127
