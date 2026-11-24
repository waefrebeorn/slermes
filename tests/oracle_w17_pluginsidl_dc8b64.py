#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_idle_tasks.py (w17_pluginsidl_dc8b64).

Ports the pure state/validation surface: _base_state, _NAME_RE, and
build_idle_task_spec's validation + normalization. The live spec builder is
called with a fixed ctx and a callable fn so only pure decisions are compared.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/hermes_cli/plugins_idle_tasks.py"
spec = importlib.util.spec_from_file_location("w17_pluginsidl_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w17_pluginsidl_live"] = live
spec.loader.exec_module(live)


def ctx():
    return types.SimpleNamespace(
        manifest=types.SimpleNamespace(name="testplug"), plugin_id="testplug")


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "base_state":
            py = live._base_state(r.get("plugin_id", ""), r.get("name", ""))
        elif func == "valid_name":
            py = bool(live._NAME_RE.match(r.get("name", "")))
        elif func == "build_spec":
            try:
                sp = live.build_idle_task_spec(
                    ctx(), r.get("name", ""), lambda rr: None,
                    interval_hours=r.get("interval", ""),
                    min_idle_hours=r.get("min_idle", ""),
                    first_run=r.get("first_run", "defer"))
                py = {"ok": True, "name": sp.name, "interval": sp.interval_hours,
                      "idle": sp.min_idle_hours, "first_run": sp.first_run}
            except ValueError:
                py = {"ok": False}
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w17_pluginsidl_dc8b64: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
