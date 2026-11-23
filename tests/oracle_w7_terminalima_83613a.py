#!/usr/bin/env python3
"""Differential oracle for agent/terminal_image.py (gate w7_terminalima_83613a)."""
import importlib.util
import json
import os
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

SRC = QUARRY + "/agent/terminal_image.py"
spec = importlib.util.spec_from_file_location("w7_terminalima_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

_ENV_KEYS = ("TERM", "TERM_PROGRAM", "KITTY_WINDOW_ID", "ITERM_SESSION_ID", "WEZTERM_PANE")


class _Image:
    def __init__(self, w, h):
        self.width, self.height = w, h


def _with_env(r, fn):
    saved = {k: os.environ.get(k) for k in _ENV_KEYS}
    try:
        for k in _ENV_KEYS:
            v = r.get(k, "")
            if v:
                os.environ[k] = v
            else:
                os.environ.pop(k, None)
        return fn()
    finally:
        for k, v in saved.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v


def _norm(x):
    if isinstance(x, tuple): return [_norm(v) for v in x]
    if isinstance(x, list): return [_norm(v) for v in x]
    if isinstance(x, dict): return {str(k): _norm(v) for k, v in x.items()}
    return x

def compute(r):
    func = r.get("func")
    if func == "kitty_color_hex":
        return _norm(live.kitty_color_hex(int(r.get("image_id", "0"))))
    if func == "cell_box":
        return _norm(live.cell_box(_Image(int(r.get("width", "0")), int(r.get("height", "0")))))
    if func == "kitty_apc":
        return _norm(live._kitty_apc(r.get("ctrl", ""), r.get("data", "")))
    if func == "grid_for":
        return _norm(live._grid_for(int(r.get("width", "0")), int(r.get("height", "0")),
                              max_cols=int(r.get("max_cols", "0")),
                              max_rows=int(r.get("max_rows", "0"))))
    if func == "kitty_placeholder_rows":
        return _norm(live.kitty_placeholder_rows(int(r.get("cols", "0")), int(r.get("rows", "0"))))
    if func == "detect_graphics":
        return _with_env(r, live.detect_terminal_graphics)
    if func == "supports_kitty_placeholders":
        return _with_env(r, live.supports_kitty_placeholders)
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

print("ORACLE w7_terminalima_83613a: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
