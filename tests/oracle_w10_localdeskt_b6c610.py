#!/usr/bin/env python3
"""Differential oracle for gateway/local_desktop_ui.py (gate w10_localdeskt_b6c610)."""
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

SRC = QUARRY + "/gateway/local_desktop_ui.py"
NAME = "local_desktop_ui_live_w10_localdeskt"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


class FakeTime:
    def __init__(self, seq):
        self.seq = list(seq)
        self.i = 0

    def monotonic(self):
        v = self.seq[min(self.i, len(self.seq) - 1)]
        self.i += 1
        return v


def compute(r):
    func = r.get("func")
    if func == "theme":
        ot, oa, oar = live.time, live._ask, live._ask_result
        try:
            live.time = FakeTime([0.0, r.get("elapsed", 0.0)])
            live._ask = lambda t, a, m, p, to: (
                r.get("apply_answer", "") if m == "theme.apply" else "")
            live._ask_result = lambda t, a, m, p, to: (
                r.get("pick") if m == "theme.pick" else None)
            return live._theme(None, None, r.get("payload") or {})
        finally:
            live.time, live._ask, live._ask_result = ot, oa, oar
    if func == "read_block":
        oa = live._ask
        try:
            live._ask = lambda t, a, m, p, to: p
            f = live._read_block(None, None, "terminal.read", 30)
            return f(r.get("start"), r.get("count"))
        finally:
            live._ask = oa
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
              file=sys.stderr)
print("ORACLE w10_localdeskt_b6c610: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
