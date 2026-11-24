#!/usr/bin/env python3
"""Differential oracle for tools/delegate_tool_sessions.py (gate w16_delegateto_87a492)."""
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
G = "w16_delegateto_87a492"
sys.path.insert(0, QUARRY)

_pkg = types.ModuleType("tools")
_pkg.__path__ = [QUARRY + "/tools"]
sys.modules["tools"] = _pkg

_spec = importlib.util.spec_from_file_location(
    "tools.delegate_tool_sessions", QUARRY + "/tools/delegate_tool_sessions.py")
live = importlib.util.module_from_spec(_spec)
sys.modules["tools.delegate_tool_sessions"] = live
_spec.loader.exec_module(live)


class _Sessions:
    """Minimal store surface used by the live lookup: subagent_session_ids + get."""
    def __init__(self, candidates, rows):
        self.candidates = list(candidates)
        self.rows = rows

    def subagent_session_ids(self, subagent_id, key):
        assert subagent_id and key == live.SUBAGENT_ID_KEY
        return list(self.candidates)

    def get(self, sid):
        d = self.rows.get(sid)
        if d is None:
            return None
        return types.SimpleNamespace(
            session_id=d.get("session_id"),
            parent_session_id=d.get("parent_session_id"),
            model_config=d.get("model_config") or {},
            source=d.get("source"),
            end_reason=d.get("end_reason"),
        )


class _Store:
    def __init__(self, candidates, rows):
        self.sessions = _Sessions(candidates, rows)


def live_out(rec):
    a = rec.get("args") or {}
    store = _Store(a.get("candidates") or [], a.get("rows") or {})
    return live.stored_child_session(store, a.get("subagent_id") or "",
                                     a.get("conversation_ids") or [])


def main():
    cases = mism = 0
    allowed = ("stored_child_session",)
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
