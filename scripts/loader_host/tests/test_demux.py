"""Tests for the byte-level frame/ASCII demultiplexer."""

from __future__ import annotations

from loader_host.binary_proto import (
    MAGIC0,
    MAGIC1,
    MAX_PAYLOAD,
    Cmd,
    Frame,
    make_request,
    make_telemetry,
)
from loader_host.demux import Demux
from loader_host.model import LoaderSnapshot


class FakeClock:
    def __init__(self) -> None:
        self.t = 1000.0

    def __call__(self) -> float:
        return self.t

    def advance(self, dt: float) -> None:
        self.t += dt


def _demux(clock=None) -> Demux:
    return Demux(frame_timeout_s=0.05, clock=clock or FakeClock())


# ---------------------------------------------------------------------------
# Pure ASCII passthrough
# ---------------------------------------------------------------------------


def test_ascii_only_stream_passes_through():
    d = _demux()
    ascii_out, frames = d.feed(b"state  : IDLE\r\n")
    assert frames == []
    assert ascii_out == b"state  : IDLE\r\n"
    assert not d.in_frame


def test_no_magic_means_no_frames():
    d = _demux()
    _, frames = d.feed(b"hello world, no magic here\n")
    assert frames == []
    assert d.stats.frames_ok == 0


# ---------------------------------------------------------------------------
# Frame extraction
# ---------------------------------------------------------------------------


def test_single_frame_decoded():
    d = _demux()
    raw = make_request(seq=1, cmd=Cmd.PING)
    ascii_out, frames = d.feed(raw)
    assert ascii_out == b""
    assert len(frames) == 1
    assert frames[0].seq == 1
    assert d.stats.frames_ok == 1


def test_frame_with_payload_decoded():
    d = _demux()
    raw = make_request(seq=5, cmd=Cmd.SET_MODE, payload=b"\x01")
    _, frames = d.feed(raw)
    assert len(frames) == 1
    assert frames[0].payload == b"\x01"


def test_two_frames_in_one_chunk():
    d = _demux()
    raw = make_request(seq=1, cmd=Cmd.PING) + make_request(seq=2, cmd=Cmd.PING)
    _, frames = d.feed(raw)
    assert [f.seq for f in frames] == [1, 2]


def test_frame_split_across_chunks():
    """A frame arriving one byte at a time is the common serial case."""
    d = _demux()
    raw = make_request(seq=3, cmd=Cmd.GET_STATUS)
    frames = []
    for byte in raw:
        _, got = d.feed(bytes([byte]))
        frames.extend(got)
    assert len(frames) == 1
    assert frames[0].seq == 3


def test_magic_split_across_chunk_boundary():
    d = _demux()
    raw = make_request(seq=7, cmd=Cmd.PING)
    _, f1 = d.feed(raw[:1])  # just MAGIC0
    assert f1 == []
    assert d.in_frame
    _, f2 = d.feed(raw[1:])
    assert len(f2) == 1
    assert f2[0].seq == 7


def test_telemetry_frame_then_ascii_reply():
    """Mixed traffic: a frame immediately followed by a text line."""
    d = _demux()
    snap = LoaderSnapshot(current_measurement=1.0, voltage_measurement=12.0)
    raw = make_telemetry(seq=4, snap=snap) + b"lmeas\r\n"
    ascii_out, frames = d.feed(raw)
    assert len(frames) == 1
    assert frames[0].cmd == 0x98
    assert ascii_out == b"lmeas\r\n"


# ---------------------------------------------------------------------------
# Resync / error handling
# ---------------------------------------------------------------------------


def test_pseudo_magic_byte_is_returned_to_ascii():
    """A lone 0xA5 followed by text must not swallow the text byte."""
    d = _demux()
    ascii_out, frames = d.feed(b"\xa5D")
    assert frames == []
    assert ascii_out == b"D"
    assert d.stats.pseudo_magic == 1


def test_repeated_magic0_keeps_waiting():
    d = _demux()
    raw = bytes([MAGIC0, MAGIC0]) + make_request(seq=9, cmd=Cmd.PING)
    _, frames = d.feed(raw)
    assert len(frames) == 1
    assert frames[0].seq == 9


def test_crc_error_counted_and_frame_dropped():
    d = _demux()
    raw = bytearray(make_request(seq=1, cmd=Cmd.PING))
    raw[-1] ^= 0xFF  # corrupt CRC
    _, frames = d.feed(bytes(raw))
    assert frames == []
    assert d.stats.crc_errors == 1


def test_bad_length_aborts_and_recovers():
    """An over-long LEN field must not desync the next good frame."""
    d = _demux()
    bogus = bytes([MAGIC0, MAGIC1, 0x00, 0x10, MAX_PAYLOAD + 1])
    good = make_request(seq=2, cmd=Cmd.PING)
    _, frames = d.feed(bogus + good)
    assert d.stats.bad_length == 1
    assert len(frames) == 1
    assert frames[0].seq == 2


def test_timeout_aborts_half_frame_and_returns_to_shell():
    clock = FakeClock()
    d = _demux(clock)
    raw = make_request(seq=1, cmd=Cmd.GET_STATUS)
    d.feed(raw[:4])  # start of a frame, then the cable is yanked
    assert d.in_frame

    clock.advance(0.2)  # > 50 ms gap
    d.tick()
    assert not d.in_frame
    assert d.stats.resyncs == 1

    # The next good frame still decodes.
    _, frames = d.feed(make_request(seq=8, cmd=Cmd.PING))
    assert len(frames) == 1


def test_tick_within_timeout_keeps_frame_alive():
    clock = FakeClock()
    d = _demux(clock)
    raw = make_request(seq=1, cmd=Cmd.GET_STATUS)
    d.feed(raw[:4])
    clock.advance(0.01)  # still inside the 50 ms window
    d.tick()
    assert d.in_frame
    assert d.stats.resyncs == 0


def test_tick_when_idle_is_a_noop():
    clock = FakeClock()
    d = _demux(clock)
    d.tick()
    assert d.stats.resyncs == 0


def test_stats_as_dict_has_expected_keys():
    d = _demux()
    assert set(d.stats.as_dict()) == {
        "frames_ok",
        "crc_errors",
        "bad_length",
        "pseudo_magic",
        "resyncs",
    }


def test_zero_length_payload_frame():
    """LEN == 0 must skip straight to CRC, not stall in PAYLOAD."""
    d = _demux()
    raw = Frame(seq=11, cmd=int(Cmd.PING), payload=b"").encode()
    _, frames = d.feed(raw)
    assert len(frames) == 1
    assert frames[0].payload == b""
