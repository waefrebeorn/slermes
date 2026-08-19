"""tests/watercup/watercup_py.py — Python trace-cup tracer.

Mirrors tests/watercup/watercup.h EXACTLY: same event alphabet
('B<d>:<0|1>', 'L<d>', 'R<d>', 'r<d>', 'V<hash>'), same FNV-1a fold.
A faithful port and LIVE Python emit identical cup hashes over the same
inputs because both run the SAME annotated control structure.

Usage (mirror the C macros inside the ported function):

    from tests.watercup.watercup_py import Cup
    def my_fn(x):
        c = Cup("my_fn")
        c.branch(x > 0)
        if x > 0:
            c.val(x)
            while x > 0:
                c.loop()
                x -= 1
        c.val(x)
        return c.finalize()   # hex string
"""
import sys

_MASK = 0xFFFFFFFFFFFFFFFF
_OFFSET = 0xcbf29ce484222325
_PRIME = 0x100000001b3


def _fnv(data: bytes) -> int:
    h = _OFFSET
    for b in data:
        h ^= b
        h = (h * _PRIME) & _MASK
    return h


class Cup:
    def __init__(self, fn: str):
        # Chain fn then "|" into ONE running FNV-1a hash — must match
        # wcup_begin() in watercup.h (which mixes fn, then "|").
        self.h = _fnv((fn + "|").encode())
        self.depth = 0

    def _mix(self, s: bytes):
        """FNV-1a fold — MUST match wcup_mix() in watercup.h byte-for-byte."""
        for b in s:
            self.h ^= b
            self.h = (self.h * _PRIME) & _MASK

    def branch(self, taken: bool):
        self._mix(b"B%d:%c" % (self.depth, ord('1' if taken else '0')))

    def loop(self):
        self._mix(b"L%d" % self.depth)

    def rec_in(self):
        self.depth += 1
        self._mix(b"R%d" % self.depth)

    def rec_out(self):
        self._mix(b"r%d" % self.depth)
        if self.depth > 0:
            self.depth -= 1

    def val(self, obj) -> None:
        """Hash a significant intermediate value (args, accumulators,
        branch operands). Stable across runs via repr()."""
        self._mix(b"V")
        self._mix(repr(obj).encode())

    def valstr(self, s: str) -> None:
        """Hash a CANONICAL STRING value — the cross-language form. Use this
        (not val) so the C WCUP_VALSTR and Python carve identical cups."""
        self._mix(b"V")
        self._mix(s.encode())

    def finalize(self) -> str:
        return "%016x" % self.h
