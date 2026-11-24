#!/usr/bin/env python3
"""Differential oracle for agent/secret_sources/command.py (w20_command_79acbf).

Imports the live module out of the quarry, then replays every harness line
through the real helpers.  The subprocess seam is scripted: ``_is_windows``
and ``_run_helper`` are monkeypatched from the fixture so no helper ever runs.
"""
import importlib.util
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w20_command_79acbf_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "agent", "secret_sources", "command.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def expected(r):
    f = r.get("func")
    if f == "unquote_dotenv_value":
        return live.unquote_dotenv_value(r.get("raw") or "")
    if f == "parse_dotenv_map":
        return live._parse_dotenv_map(r.get("stdout") or "")
    if f == "config_schema":
        return live.CommandSource().config_schema()
    if f == "fetch_plan":
        cfg = r.get("cfg")
        cfg = cfg if isinstance(cfg, dict) else {}
        is_win = bool(r.get("is_windows"))
        stdout = r.get("helper_stdout")
        live._is_windows = lambda: is_win
        live._run_helper = lambda *a, **k: stdout
        res = live.CommandSource().fetch(cfg, Path("."))
        return {
            "error": res.error,
            "error_kind": res.error_kind.value if res.error_kind is not None else None,
            "warnings": list(res.warnings),
            "secrets": dict(res.secrets),
            "timeout": live.coerce_float(cfg.get("helper_timeout_seconds", 3.0), 3.0),
        }
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
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got), file=sys.stderr)
print("ORACLE w20_command_79acbf: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
