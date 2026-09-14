"""Single-consumer demultiplexer for the shared USART1 stream.

The device sends two kinds of traffic on the same wire:

* binary frames, delimited by the ``0xA5 0x5A`` magic pair;
* Letter Shell ASCII text (echoes, replies, the prompt).

`Demux` is a byte-level state machine that routes each byte to one of the two.
It is deliberately *stateless about intent*: there is no mode flag to flip and
no handshake to complete, so a host crash or a stray byte can never wedge the
link -- the ASCII console stays reachable at all times.

Inertness of the magic: both bytes are >= 0x80, so Letter Shell never treats
them as a key binding or an erase. A lone 0xA5 in the stream is dropped at the
cost of nothing meaningful.
"""

from __future__ import annotations

import time
from collections.abc import Callable
from dataclasses import dataclass

from .binary_proto import MAGIC0, MAGIC1, MAX_PAYLOAD, Frame, decode_frame

# Byte-level states.
_S_SHELL = 0
_S_MAGIC1 = 1
_S_SEQ = 2
_S_CMD = 3
_S_LEN = 4
_S_PAYLOAD = 5
_S_CRC = 6


@dataclass
class DemuxStats:
    frames_ok: int = 0
    crc_errors: int = 0
    bad_length: int = 0
    pseudo_magic: int = 0
    resyncs: int = 0

    def as_dict(self) -> dict[str, int]:
        return {
            "frames_ok": self.frames_ok,
            "crc_errors": self.crc_errors,
            "bad_length": self.bad_length,
            "pseudo_magic": self.pseudo_magic,
            "resyncs": self.resyncs,
        }


class Demux:
    """Route bytes to the binary frame parser or to ASCII passthrough."""

    def __init__(
        self,
        frame_timeout_s: float = 0.05,
        clock: Callable[[], float] = time.monotonic,
    ) -> None:
        self.frame_timeout_s = frame_timeout_s
        self._clock = clock
        self.stats = DemuxStats()
        self._state = _S_SHELL
        self._seq = 0
        self._cmd = 0
        self._len = 0
        self._payload = bytearray()
        self._crc_bytes = bytearray()
        self._last_frame_tick = 0.0

    # -- public API --------------------------------------------------------

    @property
    def in_frame(self) -> bool:
        return self._state != _S_SHELL

    def feed(self, data: bytes) -> tuple[bytes, list[Frame]]:
        """Consume bytes; return (ascii_bytes, completed_frames)."""
        ascii_out = bytearray()
        frames: list[Frame] = []
        now = self._clock()

        for b in data:
            in_frame, frame = self._consume(b, ascii_out)
            if frame is not None:
                frames.append(frame)
            if in_frame:
                self._last_frame_tick = now

        return bytes(ascii_out), frames

    def tick(self) -> None:
        """Abort a half-received frame after a byte-gap timeout.

        Call this periodically (e.g. once per worker iteration) so a truncated
        frame cannot leave the parser stuck in-frame forever.
        """
        if self._state == _S_SHELL:
            return
        if (self._clock() - self._last_frame_tick) > self.frame_timeout_s:
            self._reset()
            self.stats.resyncs += 1

    # -- internals ---------------------------------------------------------

    def _reset(self) -> None:
        self._state = _S_SHELL
        self._payload.clear()
        self._crc_bytes.clear()

    def _consume(self, b: int, ascii_out: bytearray) -> tuple[bool, Frame | None]:
        """Handle one byte.

        Returns ``(counts_as_frame_byte, completed_frame)``.
        """
        if self._state == _S_SHELL:
            if b == MAGIC0:
                self._state = _S_MAGIC1
                return True, None
            ascii_out.append(b)
            return False, None

        if self._state == _S_MAGIC1:
            if b == MAGIC1:
                self._state = _S_SEQ
                return True, None
            # Not a real frame. The held 0xA5 is meaningless to the shell, so
            # drop it and hand this byte to the ASCII path -- no user input is
            # lost unless it is yet another 0xA5.
            self.stats.pseudo_magic += 1
            if b == MAGIC0:
                return True, None  # stay waiting for MAGIC1
            self._state = _S_SHELL
            ascii_out.append(b)
            return False, None

        if self._state == _S_SEQ:
            self._seq = b
            self._state = _S_CMD
            return True, None

        if self._state == _S_CMD:
            self._cmd = b
            self._state = _S_LEN
            return True, None

        if self._state == _S_LEN:
            if b > MAX_PAYLOAD:
                # Framing is untrustworthy; drop everything and resync on the
                # next magic.
                self.stats.bad_length += 1
                self._reset()
                return True, None
            self._len = b
            self._payload.clear()
            self._state = _S_PAYLOAD if b else _S_CRC
            if b == 0:
                self._crc_bytes.clear()
            return True, None

        if self._state == _S_PAYLOAD:
            self._payload.append(b)
            if len(self._payload) >= self._len:
                self._crc_bytes.clear()
                self._state = _S_CRC
            return True, None

        # _S_CRC
        self._crc_bytes.append(b)
        if len(self._crc_bytes) < 2:
            return True, None

        raw = (
            bytes((MAGIC0, MAGIC1, self._seq, self._cmd, self._len))
            + bytes(self._payload)
            + bytes(self._crc_bytes)
        )
        frame: Frame | None = None
        try:
            frame = decode_frame(raw)
            self.stats.frames_ok += 1
        except Exception:
            self.stats.crc_errors += 1
        self._reset()
        return True, frame


__all__ = ["Demux", "DemuxStats"]
