#!/usr/bin/env python3
"""Differential oracle for tools/delegate_tool_progress.py (gate w16_delegateto_dd6882)."""
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
G = "w16_delegateto_dd6882"
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


pkg("tools")
pkg("agent")
stub("tools.delegate_tool_registry",
     _active_subagents={}, _active_subagents_lock=threading.Lock())
FLAGS = {"ctx_files": ""}
stub("agent.prompt_cache_boundary", register_stable_prefix=lambda p: None)
stub("agent.prompt_builder", build_context_files_prompt=lambda **kw: FLAGS["ctx_files"])
live = load("tools.delegate_tool_progress", "tools/delegate_tool_progress.py")


def live_out(rec):
    func = rec.get("func")
    if func == "normalize_event":
        key = live._normalize_event(rec.get("event"))
        return key.value if isinstance(key, live.DelegateEvent) else key
    if func == "format_duration":
        return live._format_duration(rec.get("seconds"))
    if func == "short":
        return live._short(rec.get("text") or "", int(rec.get("n", 0)))
    if func == "child_task_message":
        return live.child_task_message(rec.get("goal") or "", rec.get("context"))
    if func == "child_system_prompt":
        o = rec.get("opts") or {}
        FLAGS["ctx_files"] = o.get("context_files", "")
        return live._build_child_system_prompt(
            workspace_path=o.get("workspace_path"),
            role=o.get("role", "leaf"),
            max_spawn_depth=int(o.get("max_spawn_depth", 2)),
            child_depth=int(o.get("child_depth", 1)),
        )
    return None


def main():
    cases = mism = 0
    allowed = ("normalize_event", "format_duration", "short", "child_task_message", "child_system_prompt")
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
