"""Binary frame codec for the electronic-load host link.

Wire format (see PROTOCOL.md for the authoritative spec):

    offset  size  field
    0       1     MAGIC0 = 0xA5
    1       1     MAGIC1 = 0x5A
    2       1     SEQ            host-assigned, echoed back verbatim
    3       1     CMD
    4       1     LEN            0..192
    5       N     PAYLOAD
    5+N     1     CRC_LO         CRC-16/CCITT-FALSE over SEQ|CMD|LEN|PAYLOAD
    6+N     1     CRC_HI         (little-endian)

The magic pair was chosen because both bytes fall in the 0x80..0xFF "inert"
range for Letter Shell: they are not 0x00/0x1B/0x09/0x08/0x7F/0x0A/0x0D, so a
stray frame byte never triggers a key binding, a bell, or an erase.

NOTE: the firmware does not implement this yet. This module is complete and
unit-tested so the host needs no changes once the device side lands.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from enum import IntEnum

from . import config
from .model import LoaderSnapshot

MAGIC0 = 0xA5
MAGIC1 = 0x5A

MAX_PAYLOAD = 192


class Cmd(IntEnum):
    """Request opcodes (responses use ``RESP_FLAG | opcode``)."""

    PING = 0x01
    GET_STATUS = 0x10
    SET_SETPOINT = 0x11
    SET_MODE = 0x12
    RUN = 0x13
    STOP = 0x14
    CLEAR_FAULT = 0x15
    SET_FAN = 0x16
    GET_OUT = 0x17


RESP_FLAG = 0x80
ERROR_CMD = 0xFF
TELEMETRY_CMD = 0x98

# Setpoint channel selector inside SET_SETPOINT (0 = "active mode").
CH_ACTIVE = 0
CH_CURRENT = ord("i")
CH_VOLTAGE = ord("v")
CH_POWER = ord("p")
CH_RESISTANCE = ord("r")


# ---------------------------------------------------------------------------
# CRC-16/CCITT-FALSE  (poly 0x1021, init 0xFFFF, no reflect, xorout 0x0000)
# ---------------------------------------------------------------------------


def crc16_ccitt_false(data: bytes, crc: int = 0xFFFF) -> int:
    """Return the CRC-16/CCITT-FALSE of *data*.

    Check value: ``crc16_ccitt_false(b"123456789") == 0x29B1``.
    """
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


# ---------------------------------------------------------------------------
# Frame
# ---------------------------------------------------------------------------


class FrameError(Exception):
    """Raised when a frame cannot be decoded."""


@dataclass
class Frame:
    seq: int
    cmd: int
    payload: bytes = b""

    def encode(self) -> bytes:
        if len(self.payload) > MAX_PAYLOAD:
            raise FrameError(f"payload too long: {len(self.payload)} > {MAX_PAYLOAD}")
        header = struct.pack("<4B", MAGIC0, MAGIC1, self.seq & 0xFF, self.cmd & 0xFF)
        body = struct.pack("<B", len(self.payload)) + self.payload
        crc = crc16_ccitt_false(header[2:] + body)
        return header + body + struct.pack("<H", crc)

    @property
    def is_response(self) -> bool:
        return bool(self.cmd & RESP_FLAG) or self.cmd == ERROR_CMD

    @property
    def opcode(self) -> int:
        return self.cmd & ~RESP_FLAG


def decode_frame(buf: bytes) -> Frame:
    """Decode one complete frame (no trailing bytes allowed)."""
    if len(buf) < 7:
        raise FrameError(f"frame too short: {len(buf)}")
    if buf[0] != MAGIC0 or buf[1] != MAGIC1:
        raise FrameError("bad magic")
    seq, cmd, length = buf[2], buf[3], buf[4]
    if length > MAX_PAYLOAD:
        raise FrameError(f"bad length: {length}")
    expected = 5 + length + 2
    if len(buf) != expected:
        raise FrameError(f"length mismatch: got {len(buf)}, expected {expected}")
    payload = buf[5 : 5 + length]
    rx_crc = struct.unpack_from("<H", buf, 5 + length)[0]
    calc = crc16_ccitt_false(buf[2 : 5 + length])
    if rx_crc != calc:
        raise FrameError(f"crc mismatch: got 0x{rx_crc:04X}, want 0x{calc:04X}")
    return Frame(seq=seq, cmd=cmd, payload=payload)


# ---------------------------------------------------------------------------
# Status payload
# ---------------------------------------------------------------------------

# 5 bytes of flags then 3 pad, then 14 little-endian f32 -> 64 bytes total.
_STATUS_STRUCT = struct.Struct("<5B3x14f")
STATUS_SIZE = _STATUS_STRUCT.size  # 64

assert STATUS_SIZE == 64, STATUS_SIZE


def pack_status(snap: LoaderSnapshot) -> bytes:
    """Serialise a snapshot into the 64-byte status/telemetry payload."""

    def f(value: float | None) -> float:
        return 0.0 if value is None else float(value)

    return _STATUS_STRUCT.pack(
        int(snap.state) if snap.state is not None else 0,
        int(snap.error) if snap.error is not None else 0,
        int(snap.mode) if snap.mode is not None else 0,
        1 if snap.out_enabled else 0,
        1 if snap.fan_enabled else 0,
        f(snap.current_setpoint),
        f(snap.voltage_setpoint),
        f(snap.power_setpoint),
        f(snap.resistance_setpoint),
        f(snap.current_measurement),
        f(snap.voltage_measurement),
        f(snap.power_measurement),
        f(snap.resistance_measurement),
        f(snap.temperature_measurement),
        f(snap.out_current),
        f(snap.out_voltage),
        f(snap.out_norm),
        f(snap.fan_speed),
        f(snap.fan_target),
    )


def unpack_status(payload: bytes) -> LoaderSnapshot:
    """Parse a 64-byte status/telemetry payload into a snapshot."""
    if len(payload) != STATUS_SIZE:
        raise FrameError(f"status payload must be {STATUS_SIZE} bytes, got {len(payload)}")
    (
        state,
        error,
        mode,
        out_enabled,
        fan_enabled,
        cur_sp,
        vol_sp,
        pow_sp,
        res_sp,
        cur_meas,
        vol_meas,
        pow_meas,
        res_meas,
        temp_meas,
        out_current,
        out_voltage,
        out_norm,
        fan_speed,
        fan_target,
    ) = _STATUS_STRUCT.unpack(payload)

    from .config import LoaderError, LoaderMode, LoaderState

    return LoaderSnapshot(
        state=_try_enum(LoaderState, state),
        error=_try_enum(LoaderError, error),
        mode=_try_enum(LoaderMode, mode),
        out_enabled=bool(out_enabled),
        fan_enabled=bool(fan_enabled),
        current_setpoint=cur_sp,
        voltage_setpoint=vol_sp,
        power_setpoint=pow_sp,
        resistance_setpoint=res_sp,
        current_measurement=cur_meas,
        voltage_measurement=vol_meas,
        power_measurement=pow_meas,
        resistance_measurement=res_meas,
        temperature_measurement=temp_meas,
        out_current=out_current,
        out_voltage=out_voltage,
        out_norm=out_norm,
        fan_speed=fan_speed,
        fan_target=fan_target,
        current_is_commanded=config.USE_SOFTWARE_CURRENT_PID is False,
    )


def _try_enum(enum_cls, value: int):
    try:
        return enum_cls(value)
    except ValueError:
        return None


# ---------------------------------------------------------------------------
# Request payload builders
# ---------------------------------------------------------------------------


def pack_setpoint(channel: int, value: float) -> bytes:
    return struct.pack("<Bf", channel & 0xFF, value)


def unpack_setpoint(payload: bytes) -> tuple[int, float]:
    channel, value = struct.unpack("<Bf", payload)
    return channel, value


def pack_mode(mode) -> bytes:
    return struct.pack("<B", int(mode) & 0xFF)


def unpack_mode(payload: bytes) -> int:
    (mode,) = struct.unpack("<B", payload)
    return mode


def pack_fan(enabled: bool, percent: float) -> bytes:
    return struct.pack("<Bf", 1 if enabled else 0, percent)


def unpack_fan(payload: bytes) -> tuple[bool, float]:
    enabled, percent = struct.unpack("<Bf", payload)
    return bool(enabled), percent


def pack_error(exit_code: int) -> bytes:
    return struct.pack("<b", max(-128, min(127, int(exit_code))))


def unpack_error(payload: bytes) -> int:
    (code,) = struct.unpack("<b", payload)
    return code


def make_request(seq: int, cmd: Cmd, payload: bytes = b"") -> bytes:
    return Frame(seq=seq, cmd=int(cmd), payload=payload).encode()


def make_response(seq: int, cmd: Cmd, payload: bytes = b"") -> bytes:
    return Frame(seq=seq, cmd=int(cmd) | RESP_FLAG, payload=payload).encode()


def make_error(seq: int, exit_code: int) -> bytes:
    return Frame(seq=seq, cmd=ERROR_CMD, payload=pack_error(exit_code)).encode()


def make_telemetry(seq: int, snap: LoaderSnapshot) -> bytes:
    return Frame(seq=seq, cmd=TELEMETRY_CMD, payload=pack_status(snap)).encode()


__all__ = [
    "CH_ACTIVE",
    "CH_CURRENT",
    "CH_POWER",
    "CH_RESISTANCE",
    "CH_VOLTAGE",
    "ERROR_CMD",
    "MAGIC0",
    "MAGIC1",
    "MAX_PAYLOAD",
    "RESP_FLAG",
    "STATUS_SIZE",
    "TELEMETRY_CMD",
    "Cmd",
    "Frame",
    "FrameError",
    "crc16_ccitt_false",
    "decode_frame",
    "make_error",
    "make_request",
    "make_response",
    "make_telemetry",
    "pack_error",
    "pack_fan",
    "pack_mode",
    "pack_setpoint",
    "pack_status",
    "unpack_error",
    "unpack_fan",
    "unpack_mode",
    "unpack_setpoint",
    "unpack_status",
]
