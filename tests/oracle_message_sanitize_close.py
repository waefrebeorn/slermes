#!/usr/bin/env python3
"""Oracle for agent/message_sanitization.py:close_interrupted_tool_sequence.

Harness families (message_sanitize_close) all in one per line:
  {case, msgs:<json array>, fr:<json|null>, ret:bool, out:<json array>}
Replays the live close_interrupted_tool_sequence on the SAME messages list:
appends a synthetic assistant row when the tail is a tool message, in place,
True if added. Seeds the two deps the live fn's body resolves:
  _INTERRUPTED_PLACEHOLDER  (the hidden-placeholder sentinel)
  hidden_interrupt_placeholder_row (which returns the invisible assistant row)
  append_message  (faithful: stamps then appends; timestamp is nondeterministic
                    so the harness/compare ignore it - stamp via the live
                    stamp_message_timestamp so the row shape matches C exactly,
                    then drop the 'ts' field both sides)
The C emits the post-mutation array with any timestamp stripped.
"""
import ast
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/message_sanitization.py"
INTERRUPTED = "[interrupt: no assistant output for this turn]"


def _append_message(messages, row, **kw):
    messages.append(dict(row))
    return row


# the live fn does `from agent.message_metadata import append_message` at CALL
# time, so a bare ns seed is overridden — put it on a fake sys.modules module.
_mmd = types.ModuleType("agent.message_metadata")
_mmd.append_message = _append_message
sys.modules["agent.message_metadata"] = _mmd
sys.modules["agent"] = types.ModuleType("agent")


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {
        "_INTERRUPTED_PLACEHOLDER": INTERRUPTED,
        "append_message": _append_message,
        "hidden_interrupt_placeholder_row": (
            lambda: {"role": "assistant", "content": "",
                     "display_kind": "hidden", "api_content": INTERRUPTED}),
    }
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name == "close_interrupted_tool_sequence":
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    fn = ns["close_interrupted_tool_sequence"]

    def strip_ts(x):
        if isinstance(x, dict):
            return {k: strip_ts(v) for k, v in x.items() if k != "ts"}
        if isinstance(x, list):
            return [strip_ts(i) for i in x]
        return x

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
        print("ORACLE message_sanitize_close: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        msgs = list(c.get("in") or [])
        fr = c.get("fr")
        want_ret = fn(msgs, fr)
        want_out = strip_ts(msgs)
        if bool(c.get("ret")) != want_ret or strip_ts(c.get("out")) != want_out:
            mism += 1
            print(f"MISMATCH {c.get('case')}\n  ret C={c.get('ret')} PY={want_ret}\n"
                  f"  out C={json.dumps(strip_ts(c.get('out')))} PY={json.dumps(want_out)}")
    print(f"ORACLE message_sanitize_close: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
