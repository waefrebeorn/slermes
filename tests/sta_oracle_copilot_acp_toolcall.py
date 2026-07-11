#!/usr/bin/env python3
"""
sta_oracle_copilot_acp_toolcall.py — oracle for v560 copilot_acp_client
struct-builder ports (_build_openai_tool_call, _completion_to_stream_chunks).
Recomputes each case against LIVE Python agent/copilot_acp_client and asserts
the C JSON output matches field-by-field (not byte-identical — JSON key order
may differ, so we compare via Python dict/structural equality).
"""
import sys, os, json
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from agent.copilot_acp_client import (
    _build_openai_tool_call, _completion_to_stream_chunks,
)
from types import SimpleNamespace


def norm(obj):
    """Recursively convert a Python object / SimpleNamespace / json_t-like dict
    into JSON-sortable plain structures for structural equality."""
    if isinstance(obj, SimpleNamespace):
        return {k: norm(v) for k, v in vars(obj).items()}
    if isinstance(obj, dict):
        return {k: norm(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return [norm(x) for x in obj]
    if obj is None or isinstance(obj, (str, int, float, bool)):
        return obj
    return str(obj)


def py_build(call_id, name, arguments):
    tc = _build_openai_tool_call(call_id=call_id, name=name, arguments=arguments)
    return {
        "id": tc.id, "call_id": tc.call_id, "response_item_id": tc.response_item_id,
        "type": tc.type, "function": {"name": tc.function.name, "arguments": tc.function.arguments},
    }


def py_stream(model, usage, content, tool_calls, reasoning_content, reasoning, finish_reason):
    msg = SimpleNamespace(content=content, tool_calls=tool_calls,
                          reasoning_content=reasoning_content, reasoning=reasoning)
    comp = SimpleNamespace(model=model, usage=usage,
                           choices=[SimpleNamespace(finish_reason=finish_reason, message=msg)])
    chunks = _completion_to_stream_chunks(comp)
    data = chunks[0]
    dc = {
        "model": data.model, "usage": data.usage,
        "choices": [{
            "index": data.choices[0].index,
            "finish_reason": data.choices[0].finish_reason,
            "delta": {
                "role": data.choices[0].delta.role,
                "content": data.choices[0].delta.content,
                "tool_calls": data.choices[0].delta.tool_calls,  # None or list
                "reasoning_content": data.choices[0].delta.reasoning_content,
                "reasoning": data.choices[0].delta.reasoning,
            },
        }],
    }
    uc = {"model": chunks[1].model, "usage": chunks[1].usage, "choices": chunks[1].choices}
    return [dc, uc]


def main():
    cases = []
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            continue

    mism = 0
    total = 0
    for c in cases:
        fn = c["fn"]
        total += 1
        ok = True
        detail = ""
        if fn == "build1":
            exp = py_build("call_abc", "search", '{"q":"hi"}')
            got = norm(c["out"])
            ok = (norm(exp) == got)
            detail = f"got={got} exp={exp}" if not ok else "match"
        elif fn == "build2":
            exp = py_build("c2", "", "")
            got = norm(c["out"])
            ok = (norm(exp) == got)
            detail = f"got={got} exp={exp}" if not ok else "match"
        elif fn == "stream_with_tc":
            tcs = [
                SimpleNamespace(id="t1", type="function",
                                function=SimpleNamespace(name="search", arguments='{"q":1}')),
                SimpleNamespace(id="t2", type="function",
                                function=SimpleNamespace(name="calc", arguments='{"x":2}')),
            ]
            exp = py_stream("gpt-4",
                            {"prompt_tokens": 1, "completion_tokens": 2},
                            "hello world", tcs, "thinking", None, "stop")
            got = norm(c["out"])
            ok = (norm(exp) == got)
            detail = "match" if ok else f"got={got}\nexp={exp}"
        elif fn == "stream_no_tc":
            exp = py_stream("m", None, "hi", None, None, None, "stop")
            got = norm(c["out"])
            ok = (norm(exp) == got)
            detail = "match" if ok else f"got={got}\nexp={exp}"
        else:
            ok = False
            detail = f"unknown fn {fn}"

        if not ok:
            mism += 1
            print(f"MISMATCH [{fn}] {detail}")
        else:
            print(f"ok [{fn}] {detail}")

    print(f"\nRESULT: {total - mism}/{total} match, {mism} mismatch")
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
