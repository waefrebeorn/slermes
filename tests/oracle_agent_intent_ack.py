#!/usr/bin/env python3
"""Oracle for agent/agent_runtime_helpers.py:intent_ack_continuation_mode.

Harness family (agent_intent_ack), emitted per line:
  {mode, api_mode, model, out:<str>} -> intent_ack_continuation_mode(agent)
where agent is reconstructed duck-typed (the live fn only reads
_intent_ack_continuation, .model, .api_mode). Replays the LIVE classifier:
true-ish -> all; false-ish -> off; list -> all when a substring matches the
active model; else codex_only when api_mode==codex_responses else off.
Ast-extracts the def + _INTENT_ACK_ON/_INTENT_ACK_OFF frozensets.
"""
import ast
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/agent_runtime_helpers.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {}
    for n in ast.parse(src).body:
        if isinstance(n, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id in ("_INTENT_ACK_ON", "_INTENT_ACK_OFF")
            for t in n.targets):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name == "intent_ack_continuation_mode":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    fn = ns["intent_ack_continuation_mode"]

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE agent_intent_ack: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        mode = c.get("mode")
        # The C port encodes a config list as a ';'-joined string and splits it;
        # mirror that before replaying live (live only list-matches a true list).
        if isinstance(mode, str) and ";" in mode:
            mode = mode.split(";")
        agent = types.SimpleNamespace(
            _intent_ack_continuation=mode,
            model=c.get("model", ""),
            api_mode=c.get("api_mode", ""),
        )
        want = fn(agent)
        if c.get("out") != want:
            mism += 1
            print(f"MISMATCH mode={c.get('mode')!r} api={c.get('api_mode')!r} "
                  f"model={c.get('model')!r} C={c.get('out')!r} PY={want!r}")
    print(f"ORACLE agent_intent_ack: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
