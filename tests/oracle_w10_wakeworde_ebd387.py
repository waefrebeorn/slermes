#!/usr/bin/env python3
"""Differential oracle for gate w10_wakeworde_ebd387 (tools/wake_word_engines.py)."""
import importlib.util
import json
import pathlib
import sys
import types

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/tools/wake_word_engines.py"
spec = importlib.util.spec_from_file_location("w10wwe_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

# seams: os.path.exists / os.sep / get_hermes_home are injected per case
HOME = {"value": "."}
hc = types.ModuleType("hermes_constants")
hc.get_hermes_home = lambda: pathlib.Path(HOME["value"])
sys.modules["hermes_constants"] = hc
EXISTS = {"v": False}
live.os.sep = "\\"
live.os.path.exists = lambda p: bool(EXISTS["v"])
live._ww = lambda: types.SimpleNamespace(SAMPLE_RATE=16000)
_np = types.ModuleType("numpy")
_np.float32 = float
_np.asarray = lambda frame, dtype=None: 0.0   # waveform value is irrelevant to the fakes
sys.modules["numpy"] = _np


class Features:
    def __init__(self, frame_scores):
        self.frame_scores = frame_scores
        self.i = 0
    def process_streaming(self, data):
        emb = self.frame_scores[self.i]; self.i += 1
        return iter([emb])


class Model:
    def __init__(self, scores):
        self.scores = scores
    def process_streaming(self, emb):
        return iter(list(emb))


class Spotter:
    def __init__(self, frames):
        self.frames = frames
        self.i = 0
        self.ready = 0
    def is_ready(self, stream):
        return self.ready > 0
    def decode_stream(self, stream):
        pass
    def get_result(self, stream):
        r = self.frames[self.i][self.ready - 1]
        self.ready -= 1
        return r
    def reset_stream(self, stream):
        pass


def openwake(fs, threshold, needed, initial_streak):
    e = live._OpenWakeWordEngine.__new__(live._OpenWakeWordEngine)
    e._threshold = threshold
    e._confirm_needed = needed
    e._confirm_streak = initial_streak
    e._features = Features(fs)
    e._model = Model(None)
    results = []
    for frame in [b""] * len(fs):
        results.append(e.process(types.SimpleNamespace(tobytes=lambda: frame)))
    return {"results": results, "streak": e._confirm_streak}


def sherpa(frames, mapping):
    e = live._SherpaKwsEngine.__new__(live._SherpaKwsEngine)
    e._display_to_profile = dict(mapping or {})
    e.last_match = None
    ctr = [0]
    def make_spotter():
        s = Spotter(frames)
        return s
    # Use a mutable spotter whose is_ready drains the current frame's results.
    class SP:
        def __init__(self):
            self.fi = -1
            self.queue = []
        def is_ready(self, stream):
            if self.queue: return True
            self.fi += 1
            if self.fi < len(frames): self.queue = list(frames[self.fi])
            return bool(self.queue)
        def decode_stream(self, stream): pass
        def get_result(self, stream): return self.queue.pop(0)
        def reset_stream(self, stream): pass
    e._spotter = SP()
    e._stream = types.SimpleNamespace(accept_waveform=lambda *a, **k: None)
    results = []
    for _ in frames:
        results.append(e.process([0]))
    return {"results": results, "last_match": list(e.last_match) if e.last_match else None}


def porcupine(values):
    e = live._PorcupineEngine.__new__(live._PorcupineEngine)
    results = []
    for v in values:
        class P:
            def process(self, frame): return v
        e._porcupine = P()
        results.append(e.process([0]))
    return results


def compute(r):
    f = r.get("func")
    if f == "sub":
        return live._sub(r.get("cfg"), r.get("key", ""))
    if f == "path":
        EXISTS["v"] = bool(r.get("exists", False))
        live.os.sep = r.get("sep", "\\")
        return live._looks_like_path(r.get("value", ""))
    if f == "root":
        HOME["value"] = r.get("home", "")
        return str(live._sherpa_model_root())
    if f == "openwake":
        return openwake(r.get("scores") or [], r.get("threshold", 0.6),
                        r.get("needed", 1), r.get("streak", 0))
    if f == "sherpa":
        return sherpa(r.get("results") or [], r.get("map") or {})
    if f == "porcupine":
        return porcupine(r.get("values") or [])
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
    except BaseException as e:
        py = {"__raise__": type(e).__name__}
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w10_wakeworde_ebd387: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if (mism or not cases) else 0)
