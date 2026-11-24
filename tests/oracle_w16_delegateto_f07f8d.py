#!/usr/bin/env python3
"""Differential oracle for tools/delegate_tool_continue.py (gate w16_delegateto_f07f8d)."""
import importlib.util
import json
import sys
import threading
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w16_delegateto_f07f8d"
sys.path.insert(0, QUARRY)


def pkg(name):
    m = types.ModuleType(name)
    m.__path__ = [QUARRY + "/" + name.replace(".", "/")]
    sys.modules[name] = m
    return m


def stub(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def load(name, rel):
    spec = importlib.util.spec_from_file_location(name, QUARRY + "/" + rel)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


# Parent-package bootstrap + dependency stubs. Only deterministic decision logic is live.
tools_pkg = pkg("tools")
agent_pkg = pkg("agent")
FLAGS = {"worker": False, "paused": False, "max_spawn": 2}

cfg = stub("tools.delegate_tool_config",
           _get_max_concurrent_children=lambda: 0,
           _get_max_spawn_depth=lambda: FLAGS["max_spawn"],
           _load_config=lambda: {})
tools_pkg.delegate_tool_config = cfg
stub("tools.registry", tool_error=lambda msg, **kw: str(msg))
stub("agent.oneshot_footprint", agent_is_oneshot=lambda pa: bool(getattr(pa, "oneshot", False)))


class RuntimeSessionStore:
    pass


stub("agent.runtime_session_store", RuntimeSessionStore=RuntimeSessionStore,
     is_worker_process=lambda: FLAGS["worker"])
registry = stub("tools.delegate_tool_registry",
                _active_subagents={}, _recent_subagents={},
                _active_subagents_lock=threading.Lock(),
                _owns_subagent_record=lambda rec, pa: False,
                is_spawn_paused=lambda: FLAGS["paused"])
tools_pkg.delegate_tool_registry = registry

live = load("tools.delegate_tool_continue", "tools/delegate_tool_continue.py")
_orig_refusal = live._refusal


class _Validated(Exception):
    pass


def live_out(rec):
    func = rec.get("func")
    if func == "refusal":
        o = rec.get("opts") or {}
        FLAGS["worker"] = bool(o.get("worker", False))
        FLAGS["paused"] = bool(o.get("paused", False))
        FLAGS["max_spawn"] = int(o.get("max_spawn", 2))
        parent = types.SimpleNamespace(
            oneshot=bool(o.get("oneshot", False)),
            _delegate_depth=int(o.get("depth", 0)),
            _session_db=RuntimeSessionStore() if o.get("runtime_db", False) else None,
        )
        return _orig_refusal(parent)
    if func == "claim_resume":
        state = rec.get("state") or {}
        sid = state.get("sid") or ""
        active = state.get("active") or {}
        recent = state.get("recent") or {}
        registry._active_subagents = active
        registry._recent_subagents = recent
        owned = set()
        if state.get("own_live") and sid in active:
            owned.add(id(active[sid]))
        if state.get("own_recent") and sid in recent:
            owned.add(id(recent[sid]))
        registry._owns_subagent_record = lambda r, pa: id(r) in owned
        spec, err = live._claim_resume(sid, object())
        return {"spec": spec, "error": err}
    if func == "validate":
        args = rec.get("args") or {}
        live._refusal = lambda pa: (_ for _ in ()).throw(_Validated())
        try:
            return live.continue_subagent(args.get("subagent_id"), args.get("message"), object(), background=False)
        except _Validated:
            return None
        finally:
            live._refusal = _orig_refusal
    if func == "not_found":
        return live._NOT_FOUND.format(sid=rec.get("sid") or "")
    return None


def main():
    cases = mism = 0
    allowed = ("refusal", "claim_resume", "validate", "not_found")
    for line in sys.stdin:
        line = line.strip()
        if not line or "{" not in line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in allowed:
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
