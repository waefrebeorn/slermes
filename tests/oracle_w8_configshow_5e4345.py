#!/usr/bin/env python3
"""Differential oracle for hermes_cli/config_show.py (gate w8_configshow_5e4345).

Logic under test: _show_display_section(config).  Presentation seams are made
deterministic: `color` is identity, and `active_personality_name` raises so the
module's own `display.get('personality') or 'none'` fallback is exercised.
The six printed lines are captured and compared with the C array.
"""
import contextlib
import importlib.util
import io
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [Q + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class Colors:
    CYAN = "C"
    BOLD = "B"
    YELLOW = "Y"
    DIM = "D"


def _no_personality(config):
    raise ImportError("stub: force personality fallback")


_stub("hermes_cli.config")
_stub("hermes_cli.managed_scope")
_stub("hermes_cli.colors", Colors=Colors, color=lambda s, *a: s)
_stub("hermes_cli.personality", active_personality_name=_no_personality)

LIVE_NAME = "w8_configshow_5e4345_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/hermes_cli/config_show.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "show_display_section_ex":
        continue

    config = r.get("config", {})
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        live._show_display_section(config)
    py = buf.getvalue().splitlines()

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH show_display_section_ex %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w8_configshow_5e4345: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
