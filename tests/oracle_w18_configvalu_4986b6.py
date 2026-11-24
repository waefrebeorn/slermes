#!/usr/bin/env python3
"""Differential oracle for hermes_cli/config_value_checks.py (gate w18_configvalu_4986b6)."""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
_pkg = types.ModuleType("hermes_cli")
_pkg.__path__ = [Q + "/hermes_cli"]
sys.modules["hermes_cli"] = _pkg

spec = importlib.util.spec_from_file_location(
    "w18_configvalu_live", Q + "/hermes_cli/config_value_checks.py")
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def _make_zoneinfo(available, valid):
    m = types.ModuleType("zoneinfo")

    class ZoneInfo:
        def __init__(self, name):
            if name == "UTC":
                if not available:
                    raise Exception("no tz database")
            elif name not in valid:
                raise Exception("unknown zone")
    m.ZoneInfo = ZoneInfo
    return m


def _capture(fn, config):
    out = []
    live._issue = lambda issues, sev, msg, hint: issues.append(
        {"severity": sev, "message": msg, "hint": hint})
    fn(config, out)
    return out


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
        if func == "voice":
            py = _capture(live._validate_voice, r.get("config") or {})
        elif func == "timezone":
            sys.modules["zoneinfo"] = _make_zoneinfo(
                bool(r.get("tz_available", False)), list(r.get("valid_zones") or []))
            py = _capture(live._validate_timezone, r.get("config") or {})
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

print(f"ORACLE w18_configvalu_4986b6: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
