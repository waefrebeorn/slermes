#!/usr/bin/env python3
"""Differential oracle for hermes_cli/kanban_worker_log.py (gate w16_kanbanwork_76ce43)."""
import importlib.util
import json
import sys
import types
from pathlib import Path

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w16_kanbanwork_76ce43"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# Parent-package bootstrap + the one call-time dependency, mirrored verbatim
# from hermes_cli/kanban_db_dispatch.py::_positive_int.
_hc = types.ModuleType("hermes_cli")
_hc.__path__ = [QUARRY + "/hermes_cli"]
sys.modules["hermes_cli"] = _hc
_kdd = types.ModuleType("hermes_cli.kanban_db_dispatch")

def _positive_int(value, default, *, minimum=1):
    try:
        parsed = int(value)
    except (TypeError, ValueError):
        return default
    return parsed if parsed >= minimum else default

_kdd._positive_int = _positive_int
sys.modules["hermes_cli.kanban_db_dispatch"] = _hc.kanban_db_dispatch = _kdd
_cfg = types.ModuleType("hermes_cli.config")
_cfg.load_config = lambda *a, **k: {}
sys.modules["hermes_cli.config"] = _hc.config = _cfg

_spec = importlib.util.spec_from_file_location(
    "hermes_cli.kanban_worker_log_live", QUARRY + "/hermes_cli/kanban_worker_log.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)


def _compute(r):
    f = r.get("func")
    if f == "rotation_config":
        cfg = r.get("cfg")
        rb, bc = live.worker_log_rotation_config(cfg)
        return [rb, bc]
    if f == "rotated_path":
        return str(live._rotated_log_path(Path(str(r.get("log", ""))), int(r.get("gen", 0))))
    return None


def _canon(x):
    if isinstance(x, (list, tuple)):
        return [_canon(v) for v in x]
    if isinstance(x, dict):
        return {k: _canon(v) for k, v in x.items()}
    if isinstance(x, float) and x.is_integer():
        return int(x)
    return x


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line or "{" not in line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = _compute(r)
    got = r.get("out")
    if _canon(py) != _canon(got):
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)
print("ORACLE %s: %d cases, %d mismatches" % (G, cases, mism))
sys.exit(1 if mism or not cases else 0)
