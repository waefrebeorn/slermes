#!/usr/bin/env python3
"""Differential oracle for gate w10_checkpoints_fc7424 (tui_gateway/checkpoints.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

_TRUTHY = {"1", "true", "yes", "on"}
def is_truthy_value(value, default=False):
    if value is None:
        return default
    if isinstance(value, str):
        return value.strip().lower() in _TRUTHY
    return bool(value)
_u = types.ModuleType("utils")
_u.is_truthy_value = is_truthy_value
sys.modules["utils"] = _u
_hc = types.ModuleType("hermes_cli"); _hc.__path__ = []
sys.modules["hermes_cli"] = _hc
_cfg = types.ModuleType("hermes_cli.config")
_cfg.DEFAULT_CONFIG = {"checkpoints": {"enabled": False}}
sys.modules["hermes_cli.config"] = _cfg

NAME = "w10_checkpoints_fc7424_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "tui_gateway", "checkpoints.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "load_enabled":
        return live._load_checkpoints_enabled(r.get("cfg"))
    if f == "resolve_enabled_ex":
        ev = r.get("env_value")
        if ev is None:
            os.environ.pop("HERMES_TUI_CHECKPOINTS", None)
        else:
            os.environ["HERMES_TUI_CHECKPOINTS"] = ev if isinstance(ev, str) else str(ev)
        return live.resolve_checkpoints_enabled(r.get("cfg"))
    if f == "undo_hint_fields":
        live.undo_rollback_hint_for = lambda s, t, h: r.get("hint")
        return live.undo_hint_fields(r.get("session") or {}, r.get("rewound"), r.get("params") or {})
    return None

n = m = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if not isinstance(r, dict) or "out" not in r:
        continue
    n += 1
    py, got = compute(r), r.get("out")
    if py != got:
        m += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w10_checkpoints_fc7424: %d cases, %d mismatches" % (n, m))
sys.exit(1 if m else 0)
