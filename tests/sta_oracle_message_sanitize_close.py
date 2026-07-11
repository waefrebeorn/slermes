#!/usr/bin/env python3
"""
sta_oracle_message_sanitize_close.py — Oracle for
agent/message_sanitization.py:close_interrupted_tool_sequence.

Reads C harness JSON lines (case/ret/out) from stdin, recomputes each case
against LIVE Python (mutating an equivalent list of dicts), and compares
structurally.
"""
import sys
import os
import json

sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import agent.message_sanitization as MS  # noqa: E402


def norm(x):
    if isinstance(x, list):
        return [norm(v) for v in x]
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    return x


def main():
    data = sys.stdin.read()
    c_cases = {}
    for ln in data.splitlines():
        ln = ln.strip()
        if not ln:
            continue
        try:
            obj = json.loads(ln)
        except Exception:
            continue
        if "case" in obj:
            c_cases[obj["case"]] = obj

    def build(case_messages):
        return [dict(m) for m in case_messages]

    py = {}

    # Case 1: ends on tool, no response
    msgs = build([{"role": "user", "content": "go"}, {"role": "tool", "content": "res"}])
    r = MS.close_interrupted_tool_sequence(msgs, None)
    py["tool_no_response"] = (r, msgs)

    # Case 2: ends on tool, response with whitespace
    msgs = build([{"role": "tool", "content": "x"}])
    r = MS.close_interrupted_tool_sequence(msgs, "  partial done  ")
    py["tool_with_response"] = (r, msgs)

    # Case 3: ends on user
    msgs = build([{"role": "tool", "content": "x"}, {"role": "user", "content": "hi"}])
    r = MS.close_interrupted_tool_sequence(msgs, None)
    py["ends_on_user"] = (r, msgs)

    # Case 4: empty
    msgs = build([])
    r = MS.close_interrupted_tool_sequence(msgs, "x")
    py["empty"] = (r, msgs)

    # Case 5: ends on tool, empty response
    msgs = build([{"role": "assistant", "content": "a"}, {"role": "tool", "content": "b"}])
    r = MS.close_interrupted_tool_sequence(msgs, "")
    py["tool_empty_response"] = (r, msgs)

    order = ["tool_no_response", "tool_with_response", "ends_on_user", "empty", "tool_empty_response"]
    mism = 0
    for case in order:
        if case not in c_cases:
            print(f"MISSING C case {case}")
            mism += 1
            continue
        c = c_cases[case]
        c_ret = 1 if c.get("ret") else 0
        c_out = norm(c.get("out"))
        py_ret, py_msgs = py[case]
        py_out = norm(py_msgs)
        if c_ret == (1 if py_ret else 0) and c_out == py_out:
            print(f"ok [{case}]")
        else:
            mism += 1
            print(f"MISMATCH [{case}]")
            print(f"  C_ret={c_ret} PY_ret={int(bool(py_ret))}")
            print(f"  C ={json.dumps(c_out)[:300]}")
            print(f"  PY={json.dumps(py_out)[:300]}")
    print(f"\nRESULT: {len(order)-mism}/{len(order)} match, {mism} mismatch")
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
