#!/usr/bin/env python3
"""Differential oracle for tools/delegate_tool_registry.py (gate w16_delegateto_e7d732)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w16_delegateto_e7d732"
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
stub("agent.interrupt_compat", request_hard_interrupt=lambda *a, **k: True)
stub("tools.registry", tool_error=lambda msg, **kw: str(msg))
stub("tools.delegate_tool_auto_resume", waiting_entries=lambda pa: [], dismiss=lambda pa, sid: None)
live = load("tools.delegate_tool_registry", "tools/delegate_tool_registry.py")


class _Agent:
    """Stand-in for the child AIAgent: only the two attributes _resume_spec reads."""
    def __init__(self, d):
        d = d if isinstance(d, dict) else {}
        self._delegate_resume = d.get("_delegate_resume")
        self.session_id = d.get("session_id")


def live_out(rec):
    func = rec.get("func")
    if func == "attribution":
        state = rec.get("state") or {}
        live._active_subagents = state.get("active") or {}
        live._recent_subagents = state.get("recent") or {}
        return live.get_subagent_attribution(state.get("task_id"))
    if func == "resume_spec":
        raw = rec.get("record") or {}
        py_rec = dict(raw)
        py_rec["agent"] = _Agent(raw.get("agent"))
        return live._resume_spec(py_rec)
    if func == "public_record":
        live._active_subagents = {"only": dict(rec.get("record") or {})}
        entries = live.list_active_subagents()
        return entries[0] if entries else None
    return None


def main():
    cases = mism = 0
    allowed = ("attribution", "resume_spec", "public_record")
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
