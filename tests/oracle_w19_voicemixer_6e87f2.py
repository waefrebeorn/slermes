#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/discord/voice_mixer.py (gate w19_voicemixer_6e87f2).

numpy is the optional "voice" extra, and it is not installed beside this oracle, so the
sample-domain multiply/clip is stood in for by a tiny float array double; the values under
test are the frame cursor, the fade gain and the duck/ramp state, which the live code
computes with plain Python floats.
"""
import ast
import json
import logging
import sys
import threading
import types
import __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "plugins" / "platforms" / "discord" / "voice_mixer.py"
_ANN = __future__.annotations.compiler_flag
GATE = "w19_voicemixer_6e87f2"
_CONSTS = {"SAMPLE_RATE", "CHANNELS", "SAMPLE_WIDTH", "FRAME_LENGTH_MS", "SAMPLES_PER_FRAME",
           "FRAME_SIZE", "BYTES_PER_MS", "SILENCE_FRAME"}


class _Arr:
    __slots__ = ("v",)

    def __init__(self, v):
        self.v = list(v)

    def astype(self, dtype):
        return _Arr(self.v)

    def __add__(self, other):
        return _Arr([a + b for a, b in zip(self.v, other.v)])

    def __mul__(self, scalar):
        return _Arr([a * scalar for a in self.v])

    def tobytes(self):
        return bytes(len(self.v))


class _NP:
    int16 = "int16"
    float32 = "float32"

    @staticmethod
    def frombuffer(chunk, dtype=None):
        return _Arr(range(0))

    @staticmethod
    def clip(arr, lo, hi, out=None):
        target = out if out is not None else arr
        target.v = [lo if x < lo else (hi if x > hi else x) for x in arr.v]
        return target


NP = _NP()


class AudioSource:
    def read(self):
        return b""


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    mod = types.ModuleType("voicemixer_live")
    sys.modules["voicemixer_live"] = mod
    g = mod.__dict__
    discord_stub = types.ModuleType("discord")
    discord_stub.AudioSource = AudioSource
    g.update(dict(discord=discord_stub, logging=logging, threading=threading,
                  logger=logging.getLogger("oracle"), List=list, Optional=object,
                  TYPE_CHECKING=False, np=NP))
    for node in tree.body:
        if isinstance(node, ast.Assign):
            names = []
            for t in node.targets:
                if isinstance(t, ast.Name):
                    names.append(t.id)
                elif isinstance(t, ast.Tuple):
                    names.extend(e.id for e in t.elts if isinstance(e, ast.Name))
            if names and all(n in _CONSTS for n in names):
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                             flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.FunctionDef) and node.name == "_require_numpy":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name in ("MixerChild", "VoiceMixer"):
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    g["_require_numpy"] = lambda: NP
    return g


G = extract()
MixerChild = G["MixerChild"]
VoiceMixer = G["VoiceMixer"]

TRACE = {}


def _traced_read_frame(self):
    before = self._fade_done
    frame = _orig_read_frame(self)
    if frame is None:
        TRACE[id(self)] = None
        return None
    if self.fade_frames and self._fade_done > before:
        TRACE[id(self)] = self.gain * (self._fade_done / self.fade_frames)
    else:
        TRACE[id(self)] = self.gain
    return frame


_orig_read_frame = MixerChild.read_frame
MixerChild.read_frame = _traced_read_frame


def bin_of(hex_str):
    try:
        return bytes.fromhex(hex_str or "")
    except ValueError:
        return b""


def child_plan(r):
    c = MixerChild(b"\x00" * int(r.get("pcm_len") or 0), loop=bool(r.get("loop")),
                   gain=float(r.get("gain", 1.0)), fade_in_ms=int(r.get("fade_in_ms") or 0))
    steps = []
    for _ in range(int(r.get("frames") or 0)):
        TRACE.pop(id(c), None)
        frame = c.read_frame()
        ok = frame is not None
        steps.append({"frame": ok, "gain": TRACE.get(id(c)) if ok else None,
                      "pos": c._pos, "fade_done": c._fade_done, "finished": c._finished})
    return steps


def do_read(m):
    TRACE.clear()
    m.read()
    if m._closed:
        return {"closed": True, "silence": True}
    mixed = False
    speech = []
    for c in m._speech:
        applied = TRACE.get(id(c))
        if applied is not None:
            mixed = True
        speech.append({"gain": c.gain, "applied": applied, "pos": c._pos,
                       "fade_done": c._fade_done, "finished": c._finished})
    amb = m._ambient
    if amb is not None:
        applied = TRACE.get(id(amb))
        if applied is not None:
            mixed = True
        amb_rep = {"present": True, "gain": amb.gain, "applied": applied, "pos": amb._pos,
                   "fade_done": amb._fade_done, "finished": amb._finished}
    else:
        amb_rep = None
    return {"closed": False, "silence": not mixed, "speech_live": len(m._speech),
            "speech": speech, "ambient": amb_rep,
            "duck_release_left": m._duck_release_left,
            "speech_active": m._speech_active, "ambient_gain": m._ambient_gain}


def run_sequence(r):
    m = VoiceMixer(ambient_gain=float(r.get("ambient_gain", 0.18)),
                   duck_gain=float(r.get("duck_gain", 0.06)),
                   speech_gain=float(r.get("speech_gain", 1.0)),
                   duck_release_ms=int(r.get("duck_release_ms", 400)))
    out = []
    for op in r.get("ops") or []:
        kind = op.get("op")
        if kind == "ambient":
            gain = op.get("gain")
            m.set_ambient(bin_of(op.get("hex")), gain=None if gain is None else float(gain))
            out.append(None)
        elif kind == "ambient_clear":
            m.set_ambient(None)
            out.append(None)
        elif kind == "speech":
            gain = op.get("gain")
            m.play_speech(bin_of(op.get("hex")), gain=None if gain is None else float(gain),
                          fade_in_ms=int(op.get("fade_in_ms", 40)))
            out.append(None)
        elif kind == "stop_speech":
            m.stop_speech()
            out.append(None)
        elif kind == "cleanup":
            m.cleanup()
            out.append(None)
        elif kind == "read":
            out.append(do_read(m))
        elif kind == "status":
            out.append({"speech_active": m.speech_active, "speech_count": len(m._speech),
                        "ambient_present": m._ambient is not None, "closed": m._closed,
                        "duck_release_left": m._duck_release_left,
                        "duck_release_frames": m._duck_release_frames})
        else:
            out.append(None)
    return out


def run_case(r):
    f = r.get("func")
    if f == "sequence":
        return run_sequence(r)
    if f == "child_plan":
        return child_plan(r)
    if f == "geometry":
        return {"sample_rate": G["SAMPLE_RATE"], "channels": G["CHANNELS"],
                "sample_width": G["SAMPLE_WIDTH"], "frame_length_ms": G["FRAME_LENGTH_MS"],
                "samples_per_frame": G["SAMPLES_PER_FRAME"], "frame_size": G["FRAME_SIZE"],
                "bytes_per_ms": G["BYTES_PER_MS"],
                "silence_frame_size": len(G["SILENCE_FRAME"])}
    if f == "whole_cycle_freq":
        target = float(r.get("target", 0.0))
        seconds = float(r.get("seconds", 1.0))
        cycles = max(1, round(target * seconds))
        return cycles / seconds
    return None


FUNCS = {"sequence", "child_plan", "geometry", "whole_cycle_freq"}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        if f not in FUNCS:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
