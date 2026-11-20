#!/usr/bin/env python3
"""Generate tests/fixtures/memory_manager_flow3.jsonl — >=5 cases per ported
function (9 funcs), covering decision branches, error paths, the FULL builtin
exception hierarchy (TypeError/ValueError folding in _signature_params,
Exception swallow, and BaseException passthrough), real composition, state
mutation history, and fail-closed missing scripts.

Script semantics match the harness/oracle (FAIL-CLOSED): a MISSING script is a
RuntimeError raise spec; array = per-call queue (last repeats); single value =
constant; scripted null = None; {"__raise__":cls,"msg":...} = exception.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "memory_manager_flow3.jsonl")

CASES = []


def R(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def params(**kw):
    return {k: {"kind": v} for k, v in kw.items()}


def add(func, inp=None, seams=None, state=None):
    d = {"state": {} if state is None else state}
    d.update(inp or {})
    d["seams"] = seams or {}
    CASES.append({"func": func, "in": d})


def provider(pid, name="p1", htc="fn1"):
    return {"id": pid, "name": name, "handle_tool_call": htc}


# ── _redact_for_provider ──
add("redact_for_provider", {"values": ["hello world"], "urls": True},
    {"redact_for_egress": "hello world"})
add("redact_for_provider", {"values": ["hello world"], "urls": False},
    {"redact_for_egress": "hello world"})
add("redact_for_provider", {"values": ["data:image/png;base64,AAAA"]})
add("redact_for_provider", {"values": [""]}, {"redact_for_egress": "unused"})
add("redact_for_provider",
    {"values": [{"a": "same", "d": ["same2", 3, True, None]}]},
    {"redact_for_egress": ["same", "same2"]})
add("redact_for_provider", {"values": ["my secret ABCDEFGHIJKLMNOP"]},
    {"redact_for_egress": "masked",
     "tokenize": [{"tokens": ["ABCDEFGHIJKLMNOP"]}, {"tokens": []}],
     "redact_match": True, "redact_sub": "[REDACTED]"})
add("redact_for_provider", {"values": ["a", "b"]},
    {"redact_for_egress": ["a", "b"]})
add("redact_for_provider", {"values": ["x"]},
    {"redact_for_egress": R("SystemExit", "bye")})
add("redact_for_provider", {"values": ["x"]})  # fail-closed

# ── _redact_messages_for_egress ──
add("redact_messages", {"messages": None})
add("redact_messages", {"messages": []}, {"without_withheld_prompts": [[]]})
add("redact_messages", {"messages": [{"role": "user", "content": "hi"}]},
    {"without_withheld_prompts": [[{"role": "user", "content": "hi"}]],
     "redact_for_egress": ["HI"]})
add("redact_messages", {"messages": [{"role": "user"}]},
    {"without_withheld_prompts": R("RuntimeError", "strip")})
add("redact_messages", {"messages": [{"role": "user"}]},
    {"without_withheld_prompts": [[{"role": "user"}]],
     "redact_for_egress": R("SystemExit", "bye")})
add("redact_messages", {"messages": [{"role": "user"}]})  # fail-closed

# ── _StuckCallGate.__init__ ──
add("gate_init")
add("gate_init", state={"gate": [{"key": ["h", "p"], "calls": [["t", 1, 2]]}]})
add("gate_init", state={"gate": [], "note": "x"})
add("gate_init", state={"gate": [{"key": ["h", "p"], "calls": [],
                                  "last_ok": 5, "probe_at": 5, "skips": 2}]})
add("gate_init", state={"gate": [{"key": ["h", "p"], "calls": [["t", 1, 2]]},
                                 {"key": ["h", "q"], "calls": []}]})

# ── _StuckCallGate.enter ──
K = ["h", "p"]
add("gate_enter", {"key": K, "token": "t1", "timeout": 10, "now": 100},
    state={"gate": []})
# gate full: 16 future calls -> stuck=0, in_flight=16, skipped with skips=1
full = [[f"c{i}", 100, 200] for i in range(16)]
add("gate_enter", {"key": K, "token": "new", "timeout": 10, "now": 100},
    state={"gate": [{"key": K, "calls": full}]})
# 4 overdue calls -> stuck=4, closed, probe_at set to now -> skip skips=1
overdue = [[f"c{i}", 10, 20] for i in range(4)]
add("gate_enter", {"key": K, "token": "new", "timeout": 10, "now": 100},
    state={"gate": [{"key": K, "calls": overdue}]})
# stuck=4, probe_at old -> admit probe, call added
add("gate_enter", {"key": K, "token": "probe", "timeout": 10, "now": 200},
    state={"gate": [{"key": K, "calls": overdue, "probe_at": 100, "skips": 5}]})
# gate open with stale probe_at/skips -> admit and clear them
add("gate_enter", {"key": K, "token": "t2", "timeout": 10, "now": 100},
    state={"gate": [{"key": K, "calls": [["c0", 90, 200]],
                     "probe_at": 10, "skips": 3}]})
# last_ok suppresses stuck counting -> admit
add("gate_enter", {"key": K, "token": "t3", "timeout": 10, "now": 100},
    state={"gate": [{"key": K, "calls": overdue, "last_ok": 50}]})

# ── _StuckCallGate.leave ──
add("gate_leave", {"key": K, "token": "missing", "ok": True, "now": 100},
    state={"gate": [{"key": K, "calls": [["t1", 1, 2]]}]})
add("gate_leave", {"key": K, "token": "t1", "ok": False, "now": 100},
    state={"gate": [{"key": K, "calls": [["t1", 1, 2], ["t2", 3, 4]]}]})
add("gate_leave", {"key": K, "token": "t1", "ok": True, "now": 100},
    state={"gate": [{"key": K, "calls": [["t1", 1, 2], ["t2", 3, 4]]}]})
add("gate_leave", {"key": K, "token": "t1", "ok": True, "now": 100},
    state={"gate": [{"key": K, "calls": [["t1", 1, 2]], "last_ok": 1,
                     "probe_at": 1, "skips": 2}]})
add("gate_leave", {"key": K, "token": "t1", "ok": True, "now": 100}, state={"gate": []})

# ── MemoryManager.__init__ ──
add("manager_init", {"external_prefetch_timeout": None}, {"hermes_home_key": "h"})
add("manager_init", {"external_prefetch_timeout": 3}, {"hermes_home_key": "h"})
add("manager_init", {"external_prefetch_timeout": 0}, {"hermes_home_key": "h"})
add("manager_init", {"external_prefetch_timeout": -1}, {"hermes_home_key": "h"})
add("manager_init", {"external_prefetch_timeout": 2.5}, {"hermes_home_key": "h"})
add("manager_init", {"external_prefetch_timeout": None},
    {"hermes_home_key": R("SystemExit", "bye")})
add("manager_init", {"external_prefetch_timeout": None},
    {"hermes_home_key": R("RuntimeError", "no home")})
add("manager_init", {"external_prefetch_timeout": None})  # fail-closed

# ── MemoryManager._tool_call_keywords ──
P1 = provider("1", "p1", "fn1")
TC_STATE = {"tool_call_keywords_cache": {}}
add("tool_call_keywords", {"provider": P1}, {"signature": params(a="KEYWORD_ONLY", b="POSITIONAL_ONLY")}, TC_STATE)
add("tool_call_keywords", {"provider": P1}, {"signature": params(kw="VAR_KEYWORD")}, TC_STATE)
add("tool_call_keywords", {"provider": P1}, {"signature": R("TypeError")}, TC_STATE)
add("tool_call_keywords", {"provider": P1}, {"signature": R("UnicodeDecodeError")}, TC_STATE)
add("tool_call_keywords", {"provider": P1}, {"signature": R("RuntimeError")}, TC_STATE)
add("tool_call_keywords", {"provider": P1}, {"signature": R("SystemExit", "bye")}, TC_STATE)
add("tool_call_keywords", {"provider": P1}, state={"tool_call_keywords_cache": {"1": ["b", "a"]}})
add("tool_call_keywords", {"provider": P1}, state=dict(TC_STATE))  # fail-closed

# ── MemoryManager._drop_provider ──
DSTATE = {"providers": [{"id": "1", "name": "builtin"}, {"id": "2", "name": "ext"}],
          "tool_to_provider": {"tool_a": {"id": "1", "name": "builtin"},
                               "tool_b": {"id": "2", "name": "ext"}},
          "has_external": True, "tool_call_keywords_cache": {"2": ["x"]}}
add("drop_provider", {"provider": {"id": "2", "name": "ext"}}, state=DSTATE)
add("drop_provider", {"provider": {"id": "1", "name": "builtin"}}, state=DSTATE)
add("drop_provider", {"provider": {"id": "9", "name": "gone"}}, state=DSTATE)
add("drop_provider", {"provider": {"id": "1", "name": "builtin"}},
    state={"providers": [{"id": "1", "name": "builtin"}],
           "tool_to_provider": {}, "has_external": False,
           "tool_call_keywords_cache": {}})
add("drop_provider", {"provider": {"id": "3", "name": "ext3"}},
    state={"providers": [{"id": "2", "name": "ext2"}, {"id": "3", "name": "ext3"}],
           "tool_to_provider": {"tb": {"id": "3", "name": "ext3"}}, "has_external": True,
           "tool_call_keywords_cache": {"3": None}})

# ── MemoryManager.initialize_for_agent ──
B1 = [{"id": "1", "name": "builtin"}, {"id": "2", "name": "p1"}]
B2 = [{"id": "2", "name": "p1"}, {"id": "3", "name": "p2"}]
IA = {"kwargs": {"session_id": "s"}, "say": True}
add("initialize_for_agent", dict(IA, before=B1),
    {"initialize_all": {"providers": [{"id": "1", "name": "builtin"}], "has_external": False},
     "say": None}, state={"providers": B1})
add("initialize_for_agent", dict(IA, before=B1),
    {"initialize_all": {"providers": B1, "has_external": True}, "say": "unexpected"},
    state={"providers": B1})
add("initialize_for_agent", dict(IA, before=B2),
    {"initialize_all": {"providers": [], "has_external": False},
     "say": [None, None]}, state={"providers": B2})
add("initialize_for_agent", dict(IA, before=B1, say=False),
    {"initialize_all": {"providers": [{"id": "1", "name": "builtin"}], "has_external": False},
     "logger.warning": None}, state={"providers": B1})
add("initialize_for_agent", dict(IA, before=B1),
    {"initialize_all": R("RuntimeError", "init failed")}, state={"providers": B1})
add("initialize_for_agent", dict(IA, before=B1),
    {"initialize_all": {"providers": [], "has_external": False},
     "say": R("SystemExit", "bye")}, state={"providers": B1})
add("initialize_for_agent", dict(IA, before=B1), state={"providers": B1})  # fail-closed


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
    print("per-func min count: %d" % min(counts.values()))


if __name__ == "__main__":
    main()
