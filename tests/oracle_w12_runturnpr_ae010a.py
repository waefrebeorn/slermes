#!/usr/bin/env python3
"""Differential oracle for gate w12_runturnpr_ae010a (gateway/run_turn_progress.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w12_runturnpr_ae010a"
sys.path.insert(0, QUARRY)


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = sys.modules.get(".".join(parts[:i]))
        if parent is not None and not hasattr(parent, parts[i]):
            setattr(parent, parts[i], mod)
    return mod


for _pkg in ("agent", "gateway", "hermes_cli", "gateway.platforms"):
    if _pkg not in sys.modules:
        _seed(_pkg)

_noop = lambda *a, **k: None
_seed("agent.i18n", t=lambda *a, **k: "")
_seed("agent.interrupt_compat", _accepts_keyword=lambda *a, **k: False)
_seed("agent.replay_cleanup", strip_stale_dangerous_confirmations=_noop)
_seed("gateway.config", Platform=types.SimpleNamespace(LOCAL="local"))
_seed("gateway.media_repair", repair_explicit_computer_use_media_paths=_noop)
_seed("gateway.platforms.base", BasePlatformAdapter=type("BasePlatformAdapter", (), {
    "edit_message": _noop,
    "truncate_message": staticmethod(lambda *a, **k: []),
    "_is_partial_delivery": staticmethod(lambda r: False),
}))
_seed("gateway.platforms.helpers", typing_metadata_for_turn=_noop)
_seed("gateway.turn_context", TurnContext=type("TurnContext", (), {}))
_seed("hermes_cli.config", cfg_get=_noop)
_seed("utils", is_truthy_value=lambda *a, **k: False)

SRC = QUARRY + "/gateway/run_turn_progress.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)

MIXIN = live.GatewayTurnProgressMixin


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    func = r.get("func")
    if func == "repeat_count":
        return live._append_progress_repeat_count(r.get("message"), r.get("occurrences"))
    if func == "compact":
        return MIXIN._TaskCardState._compact(r.get("value"), r.get("limit"))
    if func == "progress_text":
        return MIXIN._progress_text(r.get("lines"))
    if func == "is_reset_marker":
        raw = r.get("raw")
        return MIXIN._is_reset_marker(tuple(raw) if isinstance(raw, list) else raw)
    if func == "absorb":
        st = types.SimpleNamespace(progress_lines=list(r.get("progress_lines") or []))
        raw = r.get("raw")
        rendered = MIXIN._progress_absorb(None, st, tuple(raw) if isinstance(raw, list) else raw)
        return {"buf": st.progress_lines, "rendered": rendered}
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
