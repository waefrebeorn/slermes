#!/usr/bin/env python3
"""Differential oracle for hermes_cli/kanban_ops.py (w8_kanbanops_540bdd gate).

DB/config/output modules are stubbed; the injected config object and `_err`
representation keep the exercised branches deterministic.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/kanban_ops.py"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


hcpkg = _mod("hermes_cli")
hcpkg.__path__ = [QUARRY + "/hermes_cli"]
for _sub in ("kanban_db", "kanban_db_connect", "kanban_db_dispatch", "kanban_db_workspace"):
    _m = _mod("hermes_cli." + _sub)
    setattr(hcpkg, _sub, _m)
CUR = {}
_mod("hermes_cli.config", load_config=lambda: CUR.get("cfg", {}))


def stub_err(msg, rc=1):
    return {"code": rc, "message": msg}


_mod("hermes_cli.kanban_output", _err=stub_err,
     _fmt_ts=lambda ts: "", _print_json=lambda *a, **k: None)

spec = importlib.util.spec_from_file_location("kanbanops_w8_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["kanbanops_w8_live"] = live
spec.loader.exec_module(live)


def compute(r):
    f = r["func"]
    if f == "kanban_config":
        CUR["cfg"] = r.get("cfg")
        return live._kanban_config()
    if f == "daemon_deprecated":
        return live._cmd_daemon(types.SimpleNamespace(force=r.get("force", False)))
    if f == "gc_retention_error":
        return live._cmd_gc(types.SimpleNamespace(
            event_retention_days=r.get("event_days", 30),
            log_retention_days=r.get("log_days", 30)))
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
        print(f"ORACLE kanbanops: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH kanbanops {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE kanbanops: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
