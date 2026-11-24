#!/usr/bin/env python3
"""Differential oracle for tools/terminal_tool_macos_open.py (w20_terminalto_4615fc).

The live module is pure: import it and replay every harness line through
``_transform_macos_open_command`` / ``_build_macos_open_raise_ladder`` /
``_osascript_app``, plus ``shlex.quote`` and ``shlex.split`` (the stdlib calls the
transform depends on).  The macOS ladder is compared as text -- nothing here
runs a subprocess.
"""
import importlib.util
import json
import os
import shlex
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

sp = importlib.util.spec_from_file_location(
    "w20_terminalto_4615fc_live", os.path.join(Q, "tools", "terminal_tool_macos_open.py"))
live = importlib.util.module_from_spec(sp)
sys.modules["w20_terminalto_4615fc_live"] = live
sp.loader.exec_module(live)


def expected(r):
    f = r.get("func")
    if f == "transform":
        return live._transform_macos_open_command(r.get("command"), r.get("system"))
    if f == "ladder":
        return live._build_macos_open_raise_ladder(r.get("app"), r.get("file"))
    if f == "quote":
        return shlex.quote(r.get("value") or "")
    if f == "osascript_app":
        return live._osascript_app(r.get("script") or "")
    if f == "flags":
        return sorted(live._MACOS_OPEN_NO_TRANSFORM_FLAGS)
    if f == "split":
        try:
            return {"ok": True, "tokens": shlex.split(r.get("command") or "")}
        except ValueError:
            return {"ok": False, "error": "ValueError"}
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s\n  py=%r\n  c =%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_terminalto_4615fc: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
