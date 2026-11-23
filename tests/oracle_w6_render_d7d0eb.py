#!/usr/bin/env python3
"""Differential oracle for tui_gateway/render.py (gate w6_render_d7d0eb).

The module delegates entirely to agent.rich_output; a stub renderer with the
fixture-selected behaviour is installed in sys.modules so the live wrapper
functions are genuinely exercised.
"""
import importlib.util
import json
import sys
import types

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

SRC = "C:/Users/eman5/hermes-agent/tui_gateway/render.py"
spec = importlib.util.spec_from_file_location("render_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["render_live"] = live
spec.loader.exec_module(live)


def install(behavior, sentinel):
    if behavior == "nomodule":
        sys.modules["agent.rich_output"] = None
        return
    mod = types.ModuleType("agent.rich_output")

    def fn(*args, **kwargs):
        if behavior == "typeerror" and "cols" in kwargs:
            raise TypeError("legacy signature: no cols")
        if behavior == "error":
            raise ValueError("renderer exploded")
        return sentinel

    if behavior != "noattr":
        mod.format_response = fn
        mod.render_diff = fn
        mod.StreamingRenderer = fn
    sys.modules["agent.rich_output"] = mod


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    if func not in ("render_message", "render_diff", "make_stream_renderer", "invoke"):
        continue
    behavior = r.get("behavior", "ok")
    sentinel = r.get("sentinel", "")
    cols = int(r.get("cols", 80))
    install(behavior, sentinel)
    try:
        if func == "render_message":
            py = live.render_message(r.get("text", ""), cols=cols)
        elif func == "render_diff":
            py = live.render_diff(r.get("text", ""), cols=cols)
        elif func == "make_stream_renderer":
            py = live.make_stream_renderer(cols=cols)
        else:
            py = live._rich(r.get("name", "format_response"), r.get("text", ""), cols=cols)
    except Exception as exc:
        print("LIVE ERROR %s %r: %r" % (func, r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w6_render_d7d0eb: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
