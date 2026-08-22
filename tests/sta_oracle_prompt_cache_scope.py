#!/usr/bin/env python3
"""sta_oracle_prompt_cache_scope.py — Python twin for
src/agent/prompt_cache_scope.c. Imports the LIVE agent/prompt_cache_scope.py
and mirrors the C ops with a fake SessionDB and a plain-namespace agent,
carrying one agent across the ops so the memo slot behaves identically:
  {"kind":"call"|"safe_call","sid":...|null,"db":bool,
   "lineage":"ok|none|empty|badroot|nonlist|raise","lineage_root":"r",
   "persist_disabled":bool}
Emits {"out": scope-or-null, "memo": [sid, db_present, scope] | null}.
"""
import importlib.util
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

PY_ROOT = str(Path.home() / ".hermes" / "hermes-agent")
if not os.path.isdir(PY_ROOT):
    PY_ROOT = "/home/wubu/hermes-agent-dev"
sys.path.insert(0, PY_ROOT)


def _load(name, relpath):
    if name in sys.modules:
        return sys.modules[name]
    spec = importlib.util.spec_from_file_location(
        name, os.path.join(PY_ROOT, relpath))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


pcs = _load("agent.prompt_cache_scope", "agent/prompt_cache_scope.py")


class FakeDB:
    def __init__(self, mode, root):
        self.mode = mode
        self.root = root

    def get_compression_lineage(self, session_id):
        if self.mode == "raise":
            raise RuntimeError("lineage walk failed")
        if self.mode == "none":
            return None
        if self.mode == "empty":
            return []
        if self.mode == "nonlist":
            return "oops"
        return [self.root]


def main():
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    agent = SimpleNamespace(
        session_id=None, _session_db=None, _persist_disabled=False)
    for c in root.get("ops", []):
        kind = c.get("kind")
        if kind in ("call", "safe_call"):
            agent.session_id = c.get("sid")
            if c.get("db"):
                agent._session_db = FakeDB(
                    c.get("lineage", "ok"), c.get("lineage_root", ""))
            else:
                agent._session_db = None
            agent._persist_disabled = bool(c.get("persist_disabled", False))
            agent._prompt_cache_scope_memo = getattr(
                agent, "_prompt_cache_scope_memo", None)
            if kind == "call":
                out = pcs.resolve_prompt_cache_scope(agent)
            else:
                out = pcs.resolve_prompt_cache_scope_safe(agent)
            memo = getattr(agent, "_prompt_cache_scope_memo", None)
            memo_out = (
                [memo[0][0], memo[0][1], memo[1]] if memo is not None else None
            )
        else:
            out = None
            memo_out = None
        print(json.dumps({"out": out, "memo": memo_out}, ensure_ascii=False))


if __name__ == "__main__":
    main()
