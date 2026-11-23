#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_cmd_fleet.py (gate w15_updatecmd_1b7d31)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg


def _stub(name, **attrs):
    m = types.ModuleType(name)
    if "." in name:
        parent = name.rsplit(".", 1)[0]
        if parent not in sys.modules:
            pm = types.ModuleType(parent)
            pm.__path__ = [QUARRY + "/" + parent.replace(".", "/")]
            sys.modules[parent] = pm
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_stub("hermes_cli.update_cmd_common", _best_effort=lambda *a, **k: None)
_stub("hermes_cli.update_host_obligation", PROFILE_MARKER_NAME="marker")
_stub("hermes_cli.update_inventory", _gateway_service_matches_profile=lambda *a, **k: False)


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


live = load("w15_updatecmd_1b7d31_live", QUARRY + "/hermes_cli/update_cmd_fleet.py")


def compute(r):
    f = r.get("func")
    if f == "is_serve_or_dashboard_unit":
        return live._is_serve_or_dashboard_unit(r.get("unit", ""))
    if f == "is_hermes_gateway_unit":
        return live._is_hermes_gateway_unit(r.get("unit", ""))
    if f == "receipt_looks_unfinished":
        return live._receipt_looks_unfinished(r.get("receipt") or {})
    if f == "restart_phase_failure_is_incomplete":
        return live._restart_phase_failure_is_incomplete(r.get("surviving"), r.get("pre_restart_pids"))
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
    try:
        py = compute(r)
    except Exception as exc:
        print("ORACLE w15_updatecmd_1b7d31: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w15_updatecmd_1b7d31 %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w15_updatecmd_1b7d31: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
