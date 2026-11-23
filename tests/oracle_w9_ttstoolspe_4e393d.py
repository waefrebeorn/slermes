#!/usr/bin/env python3
"""Differential oracle for tools/tts_tool_speaker.py (gate w9_ttstoolspe_4e393d).

The live module imports a few heavy helper modules at top level; they are stubbed
in sys.modules (with parent packages bootstrapped) so the pure byte helpers can be
called without any I/O, threading or sounddevice dependency.
"""
import importlib.util
import json
import queue
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

tools = types.ModuleType("tools"); tools.__path__ = [Q + "/tools"]; sys.modules["tools"] = tools
agent = types.ModuleType("agent"); agent.__path__ = [Q + "/agent"]; sys.modules["agent"] = agent

def _stub(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m

_stub("agent.memory_provider", ctx_bound=lambda f: f, spawn_context_thread=lambda *a, **kw: None)
_stub("tools.tts_text_normalize", _strip_markdown_for_tts=lambda s: s)
_stub("tools.tts_tool_delivery", _origin=lambda: None, _remove_quietly=lambda p: None)

SRC = Q + "/tools/tts_tool_speaker.py"
spec = importlib.util.spec_from_file_location("w9_ttstoolspe_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


class Ev:
    """Fake threading.Event; is_set() trips permanently after n calls."""
    def __init__(self, n):
        self.n = n
        self.c = 0

    def is_set(self):
        self.c += 1
        return self.n >= 0 and self.c > self.n


def compute(r):
    func = r.get("func")
    if func == "align":
        chunks = [bytes(c) for c in r.get("chunks", [])]
        res = list(live._align_int16_chunks(chunks, Ev(r.get("stop_after", -1)),
                                            pad_tail=r.get("pad_tail", True)))
        return [list(b) for b in res]
    if func == "drain":
        q = queue.Queue()
        for x in r.get("items", []):
            q.put(None if x is None else bytes(x))
        return [list(b) for b in live._drain_chunks(q)]
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)
print("ORACLE w9_ttstoolspe_4e393d: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if (mism or not cases) else 0)
