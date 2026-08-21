#!/usr/bin/env python3
"""sta_oracle_foreign_sessions_pure.py — oracle twin for foreign_sessions_pure.c.

Calls the LIVE hermes_cli/foreign_sessions.py pure helpers and emits one JSON
line per op. t_port_foreign_sessions_pure.c emits the same shape.

Fixture ops:
  {"op": "is_wrapper_text", "text": "<user_instructions>..."}
  {"op": "flatten_blocks", "content": "...", "source": "claude"}
  {"op": "merge_turns", "turns": [["user","hi"],["user","there"],["assistant","ok"]]}
  {"op": "session_label", "source": "claude", "title": "My Session", "stem": "abc"}
"""
import json
import os
import sys
import importlib.util


def _load_module():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(os.path.dirname(os.path.dirname(here)))
    cand = os.path.join(repo, "hermes_cli", "foreign_sessions.py")
    spec = importlib.util.spec_from_file_location("live_foreign_sessions", cand)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    path = sys.argv[1]
    with open(path, encoding="utf-8") as f:
        spec = json.load(f)
    mod = _load_module()
    out = []
    for case in spec.get("ops", []):
        op = case.get("op")
        if op == "is_wrapper_text":
            out.append({"op": op, "out": bool(mod._is_wrapper_text(case.get("text", "")))})
        elif op == "flatten_blocks":
            out.append({"op": op, "out": mod._flatten_blocks(case.get("content", ""), case.get("source", "claude"))})
        elif op == "merge_turns":
            out.append({"op": op, "out": mod._merge_turns([tuple(t) for t in case.get("turns", [])])})
        elif op == "session_label":
            fs = mod.ForeignSession.__new__(mod.ForeignSession)
            # reconstruct label without constructing full object
            name = {"claude": "Claude Code", "codex": "Codex CLI"}.get(case.get("source", ""), case.get("source", ""))
            title = (case.get("title") or "").strip() or case.get("stem", "")
            out.append({"op": op, "out": f"[{name}] {title[:60]}"})
    for line in out:
        sys.stdout.write(json.dumps(line, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
