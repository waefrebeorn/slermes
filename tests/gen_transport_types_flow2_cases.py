#!/usr/bin/env python3
"""Generate tests/fixtures/transport_types_flow2.jsonl for the transport_types_flow2
FLOW oracle (agent/transports/types.py UNCLAIMED remainder: the 12 dataclass-generated
methods __init__ / __repr__ / __eq__ / __replace__ x ToolCall / Usage /
NormalizedResponse). >=5 cases per ported function, exercising default/present-null
semantics, repr formatting, deep/cross-class equality, the __replace__ constructor seam,
BaseException propagation, the real unknown-kwarg TypeError and fail-closed missing
scripts.

Script semantics match the harness/oracle (FAIL-CLOSED): a MISSING script returns a
RuntimeError raise spec; array = per-call queue (last repeats, empty -> None); single
value = constant; scripted null = None; {"__raise__": cls, "msg": ...} = exception.
ASCII domain; integral JSON numbers are int literals; field values are plain JSON (the
dataclass stores them verbatim). DIRECT __init__ / __eq__ inputs supply required fields
(no binding-error cases there); __replace__ probes the unknown-kwarg TypeError path.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "transport_types_flow2.jsonl")

CASES = []


def R(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def add(func, inp=None, seams=None):
    CASES.append({"func": func, "in": {**(inp or {}), "seams": seams or {}}})


TC = {"id": "x", "name": "n", "arguments": "{}"}
NR = {"content": "c", "tool_calls": None, "finish_reason": "stop"}

# ── ToolCall.__init__ (defaults; present-null verbatim) ──
add("tool_call_init", {"fields": dict(TC)})
add("tool_call_init", {"fields": {**TC, "provider_data": {"call_id": "c1"}}})
add("tool_call_init", {"fields": {"id": None, "name": "n", "arguments": "{}"}})
add("tool_call_init", {"fields": {**TC, "provider_data": None}})
add("tool_call_init", {"fields": {"id": "a", "name": "b", "arguments": '{"k": 1}',
                                  "provider_data": {"extra_content": {"google": {"thought_signature": "s"}}}}})
add("tool_call_init", {"fields": {"id": "", "name": "", "arguments": ""}})

# ── ToolCall.__repr__ (repr=False provider_data omitted; Python str repr) ──
add("tool_call_repr", {"fields": dict(TC)})
add("tool_call_repr", {"fields": {"id": None, "name": "n", "arguments": "{}"}})
add("tool_call_repr", {"fields": {"id": "a'b", "name": "n", "arguments": "{}"}})
add("tool_call_repr", {"fields": {**TC, "provider_data": {"a": 1}}})
add("tool_call_repr", {"fields": {"id": "x", "name": "n", "arguments": '{"k": 1}'}})
add("tool_call_repr", {"fields": {**TC, "provider_data": None}})

# ── ToolCall.__eq__ (deep equality; cross-class False; integral float == int) ──
add("tool_call_eq", {"self": dict(TC), "other": dict(TC)})
add("tool_call_eq", {"self": dict(TC), "other": {**TC, "provider_data": {"a": 1}}})
add("tool_call_eq", {"self": {**TC, "provider_data": {"a": 1}},
                     "other": {**TC, "provider_data": {"a": 1}}})
add("tool_call_eq", {"self": dict(TC), "other": {"id": "y", "name": "n", "arguments": "{}"}})
add("tool_call_eq", {"self": dict(TC), "other_class": "Usage", "other": {}})
add("tool_call_eq", {"self": {**TC, "provider_data": {"a": [1, 2]}},
                     "other": {**TC, "provider_data": {"a": [1, 2]}}})
add("tool_call_eq", {"self": {**TC, "provider_data": {"a": 1}},
                     "other": {**TC, "provider_data": {"a": 1.0}}})

# ── ToolCall.__replace__ (ctor seam; fail-closed; BaseException; real TypeError) ──
add("tool_call_replace", {"self": dict(TC), "changes": {"name": "n2"}},
    {"ToolCall.__init__": None})
add("tool_call_replace", {"self": {**TC, "provider_data": {"call_id": "c"}},
                          "changes": {"provider_data": None, "id": "y"}},
    {"ToolCall.__init__": None})
add("tool_call_replace", {"self": dict(TC), "changes": {}}, {"ToolCall.__init__": None})
add("tool_call_replace", {"self": dict(TC), "changes": {"name": "z"}}, {})  # fail-closed
add("tool_call_replace", {"self": dict(TC), "changes": {"name": "z"}},
    {"ToolCall.__init__": R("SystemExit", "bye")})
add("tool_call_replace", {"self": dict(TC), "changes": {"name": "z"}},
    {"ToolCall.__init__": R("KeyboardInterrupt", "ki")})
add("tool_call_replace", {"self": dict(TC), "changes": {"name": "z"}},
    {"ToolCall.__init__": R("GeneratorExit", "ge")})
add("tool_call_replace", {"self": dict(TC), "changes": {"bogus": 1}},
    {"ToolCall.__init__": None})  # real ctor TypeError

# ── Usage.__init__ (all default 0; present-null verbatim) ──
add("usage_init", {"fields": {}})
add("usage_init", {"fields": {"prompt_tokens": 1, "completion_tokens": 2,
                              "total_tokens": 3, "cached_tokens": 4}})
add("usage_init", {"fields": {"prompt_tokens": None}})
add("usage_init", {"fields": {"prompt_tokens": 1.5}})
add("usage_init", {"fields": {"total_tokens": 10}})
add("usage_init", {"fields": {"prompt_tokens": 0, "completion_tokens": 0,
                              "total_tokens": 0, "cached_tokens": 0}})

# ── Usage.__repr__ (int / float / None formatting) ──
add("usage_repr", {"fields": {}})
add("usage_repr", {"fields": {"prompt_tokens": 1, "completion_tokens": 2,
                              "total_tokens": 3, "cached_tokens": 4}})
add("usage_repr", {"fields": {"prompt_tokens": 1.5, "completion_tokens": 2.5}})
add("usage_repr", {"fields": {"prompt_tokens": None}})
add("usage_repr", {"fields": {"total_tokens": 100}})
add("usage_repr", {"fields": {"cached_tokens": 7}})

# ── Usage.__eq__ ──
add("usage_eq", {"self": {}, "other": {}})
add("usage_eq", {"self": {"prompt_tokens": 1}, "other": {"prompt_tokens": 1}})
add("usage_eq", {"self": {"prompt_tokens": 1}, "other": {"prompt_tokens": 2}})
add("usage_eq", {"self": {"prompt_tokens": 1}, "other": {"prompt_tokens": 1.0}})
add("usage_eq", {"self": {"prompt_tokens": 1}, "other_class": "ToolCall", "other": dict(TC)})
add("usage_eq", {"self": {"prompt_tokens": 1, "completion_tokens": 2,
                          "total_tokens": 3, "cached_tokens": 4},
                 "other": {"prompt_tokens": 1, "completion_tokens": 2,
                           "total_tokens": 3, "cached_tokens": 4}})

# ── Usage.__replace__ ──
add("usage_replace", {"self": {"prompt_tokens": 1, "completion_tokens": 2,
                               "total_tokens": 3, "cached_tokens": 4},
                      "changes": {"prompt_tokens": 9}}, {"Usage.__init__": None})
add("usage_replace", {"self": {}, "changes": {"cached_tokens": 5}}, {"Usage.__init__": None})
add("usage_replace", {"self": {"prompt_tokens": 1}, "changes": {}}, {"Usage.__init__": None})
add("usage_replace", {"self": {}, "changes": {"prompt_tokens": 2}}, {})  # fail-closed
add("usage_replace", {"self": {}, "changes": {"prompt_tokens": 2}},
    {"Usage.__init__": R("SystemExit", "bye")})
add("usage_replace", {"self": {}, "changes": {"bogus": 1}}, {"Usage.__init__": None})
add("usage_replace", {"self": {}, "changes": {"prompt_tokens": 2}},
    {"Usage.__init__": R("ValueError", "v")})

# ── NormalizedResponse.__init__ ──
add("normalized_response_init", {"fields": dict(NR)})
add("normalized_response_init", {"fields": {"content": "hi", "tool_calls": [],
                                            "finish_reason": "stop", "reasoning": "r",
                                            "usage": {"prompt_tokens": 1},
                                            "provider_data": {"reasoning_details": [1]}}})
add("normalized_response_init", {"fields": {"content": None, "tool_calls": None,
                                            "finish_reason": "length"}})
add("normalized_response_init", {"fields": {"content": "c",
                                            "tool_calls": [{"id": "i", "name": "nm", "arguments": "{}"}],
                                            "finish_reason": "tool_calls"}})
add("normalized_response_init", {"fields": {**NR, "usage": None, "reasoning": None}})
add("normalized_response_init", {"fields": {"content": "", "tool_calls": [],
                                            "finish_reason": "content_filter",
                                            "provider_data": {"anthropic_content_blocks": [{"type": "text"}]}}})

# ── NormalizedResponse.__repr__ (provider_data omitted; plain-JSON nested values) ──
add("normalized_response_repr", {"fields": dict(NR)})
add("normalized_response_repr", {"fields": {"content": "c", "tool_calls": [],
                                            "finish_reason": "stop"}})
add("normalized_response_repr", {"fields": {"content": "c",
                                            "tool_calls": [{"id": "i", "name": "nm", "arguments": "{}"}],
                                            "finish_reason": "tool_calls"}})
add("normalized_response_repr", {"fields": {**NR, "usage": {"prompt_tokens": 1,
                                                            "completion_tokens": 2,
                                                            "total_tokens": 3,
                                                            "cached_tokens": 0}}})
add("normalized_response_repr", {"fields": {"content": None, "tool_calls": None,
                                            "finish_reason": "length", "reasoning": "r"}})
add("normalized_response_repr", {"fields": {**NR, "provider_data": {"x": 1}}})

# ── NormalizedResponse.__eq__ ──
add("normalized_response_eq", {"self": dict(NR), "other": dict(NR)})
add("normalized_response_eq", {"self": dict(NR),
                               "other": {"content": "c", "tool_calls": None, "finish_reason": "length"}})
add("normalized_response_eq", {"self": {"content": "c", "tool_calls": [{"id": "i", "name": "n", "arguments": "{}"}], "finish_reason": "stop"},
                               "other": {"content": "c", "tool_calls": [{"id": "i", "name": "n", "arguments": "{}"}], "finish_reason": "stop"}})
add("normalized_response_eq", {"self": {**NR, "usage": {"prompt_tokens": 1}},
                               "other": {**NR, "usage": {"prompt_tokens": 1.0}}})
add("normalized_response_eq", {"self": dict(NR), "other_class": "Usage", "other": {}})
add("normalized_response_eq", {"self": {**NR, "provider_data": {"a": 1}},
                               "other": {**NR, "provider_data": {"a": 1}}})

# ── NormalizedResponse.__replace__ ──
add("normalized_response_replace", {"self": dict(NR), "changes": {"finish_reason": "length"}},
    {"NormalizedResponse.__init__": None})
add("normalized_response_replace", {"self": {**NR, "reasoning": "r"}, "changes": {"usage": None}},
    {"NormalizedResponse.__init__": None})
add("normalized_response_replace", {"self": dict(NR), "changes": {}},
    {"NormalizedResponse.__init__": None})
add("normalized_response_replace", {"self": dict(NR), "changes": {"content": "d"}}, {})  # fail-closed
add("normalized_response_replace", {"self": dict(NR), "changes": {"content": "d"}},
    {"NormalizedResponse.__init__": R("SystemExit", "bye")})
add("normalized_response_replace", {"self": dict(NR), "changes": {"bogus": 1}},
    {"NormalizedResponse.__init__": None})  # real ctor TypeError
add("normalized_response_replace", {"self": dict(NR),
                                    "changes": {"reasoning": "new", "provider_data": {"k": "v"}}},
    {"NormalizedResponse.__init__": None})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print("wrote %s: %d cases, %d funcs" % (OUT, len(CASES), len(counts)))
    if short:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % short)
    if len(counts) != 12:
        raise SystemExit("EXPECTED 12 FUNCS, got %d" % len(counts))
    print("per-func min count: %d" % min(counts.values()))


if __name__ == "__main__":
    main()