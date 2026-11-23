#!/usr/bin/env python3
"""Differential oracle for gate w12_sessionexe_68345d (gateway/session_execution.py)."""
import importlib.util
import json
import sys
import threading
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w12_sessionexe_68345d"
sys.path.insert(0, QUARRY)
NS = types.SimpleNamespace


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


for _pkg in ("agent", "gateway", "hermes_cli", "utils", "gateway.platforms"):
    if _pkg not in sys.modules:
        _seed(_pkg)
_noop = lambda *a, **k: None
_seed("agent.interrupt_compat", _accepts_keyword=lambda *a, **k: False)
_seed("agent.replay_cleanup", strip_stale_dangerous_confirmations=_noop)
_seed("gateway.config", Platform=NS(LOCAL="local"))
_seed("gateway.media_repair", repair_explicit_computer_use_media_paths=_noop)
_seed("gateway.platforms.base", BasePlatformAdapter=type("BasePlatformAdapter", (), {}))
_seed("gateway.session_api_turn", api_runtime_locked=lambda *a, **k: None)
_seed("gateway.turn_context", TurnContext=type("TurnContext", (), {}))
_seed("hermes_cli.config", cfg_get=_noop)
_seed("utils", is_truthy_value=lambda *a, **k: False)

POLICY = [None]
_seed("gateway.session_policy", policy_for_source=lambda runner, source: POLICY[0])

SRC = QUARRY + "/gateway/session_execution.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)
MIXIN = live.GatewaySessionAgentMixin


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    global POLICY
    func = r.get("func")
    if func == "skip_context_files":
        policy = r.get("policy")
        POLICY[0] = NS(ignore_rules=policy["ignore_rules"]) if isinstance(policy, dict) and policy else None
        fake = NS(
            _runner=NS(),
            _ctx=NS(user_config={"gateway": {"platforms": r.get("platforms_cfg")}}, source=NS()),
        )
        return MIXIN._skip_context_files(fake, r.get("platform_key"))
    if func == "current_message_count":
        sid = r.get("session_id")
        row = r.get("row")
        row_obj = NS(**row) if isinstance(row, dict) else None
        store = NS(sessions={sid: row_obj}) if r.get("has_store") else None
        fake = NS(_runner=NS(persistence=store), _ctx=NS(session_id=sid))
        return MIXIN._current_message_count(fake)
    if func == "cached_sid_is_dead":
        key = r.get("session_key") or "key"
        entry = r.get("cache_entry")
        lock = threading.Lock() if r.get("has_lock") else None
        cache = {key: tuple(entry)} if (lock is not None and entry is not None) else None
        fake = NS(
            _ctx=NS(session_key=key, session_id=r.get("session_id")),
            _runner=NS(session_store=NS(_is_session_ended_in_db=lambda sid, session_key: bool(r.get("dead_check")))),
        )
        peek_sid, dead = MIXIN._cached_sid_is_dead(fake, lock, cache)
        return {"peek_sid": peek_sid, "dead": dead}
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
