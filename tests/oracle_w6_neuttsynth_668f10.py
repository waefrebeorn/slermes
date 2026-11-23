#!/usr/bin/env python3
"""Differential oracle for tools/neutts_synth.py (gate w6_neuttsynth_668f10).

NumPy is not installed on this box, so the module's single deterministic
computation (_write_wav) is driven through a tiny float32/int16-compatible
numpy stand-in registered in sys.modules. The live _write_wav body -- its
struct.pack WAV header and truncation rules -- is what is actually compared.
"""
import importlib.util
import json
import os
import struct
import sys
import tempfile
import types

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def _f32(v):
    return struct.unpack("<f", struct.pack("<f", float(v)))[0]


class _Arr:
    def __init__(self, vals):
        self.vals = list(vals)

    def __len__(self):
        return len(self.vals)

    def flatten(self):
        return self

    def __mul__(self, other):
        return _Arr([_f32(v * other) for v in self.vals])

    def astype(self, _dtype):
        return _Arr([int(v) for v in self.vals])  # int16 range; truncation toward zero

    def tobytes(self):
        return b"".join(struct.pack("<h", v) for v in self.vals)


_fake = types.ModuleType("numpy")
_fake.ndarray = _Arr
_fake.float32 = type("float32", (), {})
_fake.int16 = type("int16", (), {})
_fake.array = lambda x, dtype=None: _Arr([_f32(v) for v in x])
_fake.clip = lambda a, lo, hi: _Arr([min(max(v, lo), hi) for v in a.vals])
sys.modules["numpy"] = _fake

SRC = QUARRY + "/tools/neutts_synth.py"
spec = importlib.util.spec_from_file_location("w6_neuttsynth_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def wav_bytes(r):
    samples = r.get("samples", [])
    sr = int(r.get("sample_rate", "24000"))
    fd, path = tempfile.mkstemp(suffix=".wav")
    os.close(fd)
    try:
        live._write_wav(path, samples, sr)
        with open(path, "rb") as fh:
            return list(fh.read())
    finally:
        os.unlink(path)


def compute(r):
    func = r.get("func")
    if func == "pcm":
        data = wav_bytes(r)
        return [struct.unpack("<h", bytes(data[44 + 2*i:46 + 2*i]))[0]
                for i in range((len(data) - 44) // 2)]
    if func == "wav":
        return wav_bytes(r)
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s %r" % (r.get("func"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)

print("ORACLE w6_neuttsynth_668f10: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
