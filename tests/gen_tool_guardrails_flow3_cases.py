#!/usr/bin/env python3
"""Generate tests/fixtures/tool_guardrails_flow3.jsonl for the tg3 FLOW port.

Each line: {"func": <live_name>, "in": {...inputs, "self": {...}, "io": {...}}}.
Script semantics (shared with the C harness dispatcher and the oracle Script):
  - absent key / exhausted queue -> fail-closed RuntimeError "no script for <name>"
  - non-list value        -> constant result reused every call
  - list value            -> per-call queue (exhausted -> fail-closed)
  - {"__raise__":K,"message":M} -> raised exception of kind K
  - {"__script__":true,"ret":X}  -> list-valued result X (queue-ambiguity wrap)
  - null                  -> None result (delivered as a NULL pointer in C)

>=5 cases per unclaimed function (11 functions). Covers the full builtin
exception hierarchy on _int_at_least's `except (TypeError, ValueError)`, the
streak / batch-cycle / hard-stop / stub paths of observe_call, and the state
carrier projections of the controller methods.
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "tool_guardrails_flow3.jsonl"

CASES = []
FUNCS = [
    "is_stall_guard_repeatable",
    "_is_non_interactive_platform",
    "_canonical_json",
    "_int_at_least",
    "_ordinal",
    "_without_execute_code_metadata",
    "ToolCallGuardrailController.__init__",
    "ToolCallGuardrailController._decide",
    "ToolCallGuardrailController.observe_call",
    "ToolCallGuardrailController._detect_identical_cycle",
    "ToolCallGuardrailController.record_persisted_result",
]


def add(func, inp, io=None, self_state=None):
    d = dict(inp)
    d["io"] = io or {}
    d["self"] = self_state or {}
    CASES.append({"func": func, "in": d})


def R(kind, msg=""):
    return {"__raise__": kind, "message": msg}


def sig(tn, ah):
    return {"tool_name": tn, "args_hash": ah}


def entry(s, rh, rep=False):
    return [s, rh, rep]


# ------------------------------------------------------- is_stall_guard_repeatable
F = "is_stall_guard_repeatable"
for tn in ["process_manage", "foo_get_result", "foo_poll", "read_file",
           "x_poll_extra", "poll", "", "terminal", "srv_get_result"]:
    add(F, {"tool_name": tn})

# --------------------------------------------------- _is_non_interactive_platform
F = "_is_non_interactive_platform"
for p in ["gateway", "cron", "CLI", " tui ", "", None, 5, "api_server",
          "  custom  ", "SubAgent", "desktop"]:
    add(F, {"platform": p})

# --------------------------------------------------------------- _canonical_json
F = "_canonical_json"
for v in [
    {"b": 1, "a": 2},
    [1, "x", True, None],
    "héllo",
    {"z": {"y": 1, "a": [3, 2]}},
    {"lower": False, "n": 0},
    [],
    {},
]:
    add(F, {"value": v})

# ----------------------------------------------------------------- _int_at_least
F = "_int_at_least"
# value, default, minimum, py_int result (or raise)
for value, dflt, mn, iv in [
    (5, 2, 1, 5),          # >= minimum -> 5
    (0, 2, 1, 0),          # below minimum -> default 2
    ("junk", 3, 0, R("ValueError", "bad")),   # caught -> default
    (None, 7, 0, R("TypeError", "none")),     # caught -> default
    (1, 4, 1, R("SystemExit", "bye")),        # NOT caught -> propagate
    (1, 4, 1, R("KeyboardInterrupt", "ki")),  # NOT caught -> propagate
    (1, 4, 1, R("OverflowError", "ovf")),     # ArithmeticError, not (T,V) -> propagate
    (1, 4, 1, R("UnicodeDecodeError", "u")),  # ValueError subclass -> caught -> default
    (0, 9, 0, 0),          # 0 disables (minimum 0) -> 0
    (12, 9, 5, 12),        # positive -> 12
    (1, 4, 1, R("GeneratorExit", "ge")),      # NOT caught -> propagate
]:
    add(F, {"value": value, "default": dflt, "minimum": mn}, {"py_int": iv})
add(F, {"value": 1, "default": 3, "minimum": 1}, {})  # fail-closed -> RuntimeError

# ---------------------------------------------------------------------- _ordinal
F = "_ordinal"
for c in [1, 2, 3, 4, 11, 12, 13, 21, 22, 23, 101, 0]:
    add(F, {"count": c})

# ----------------------------------------------- _without_execute_code_metadata
F = "_without_execute_code_metadata"
for parsed in [
    5,
    "text",
    None,
    {"duration_seconds": 1.2, "kernel": {"execution_count": 7}, "out": "ok"},
    {"kernel": {"execution_count": 7, "status": "ok"}},
    {"duration_seconds": 0.1, "x": 1},
    {"a": 1, "b": 2},
    {"kernel": "not-a-dict", "duration_seconds": 9},
]:
    add(F, {"parsed": parsed})

# ------------------------------------------------------------ __init__
F = "ToolCallGuardrailController.__init__"
DEFAULT_CFG = {"warnings_enabled": True, "hard_stop_enabled": False}
for cfg, io in [
    (None, {"ToolCallGuardrailConfig": DEFAULT_CFG, "reset_for_turn": None}),
    ({}, {"ToolCallGuardrailConfig": DEFAULT_CFG, "reset_for_turn": None}),
    ({"warnings_enabled": False}, {"ToolCallGuardrailConfig": DEFAULT_CFG, "reset_for_turn": None}),
    (0, {"ToolCallGuardrailConfig": DEFAULT_CFG, "reset_for_turn": None}),
    ("cfg-marker", {"ToolCallGuardrailConfig": DEFAULT_CFG, "reset_for_turn": None}),
    (None, {"ToolCallGuardrailConfig": DEFAULT_CFG, "reset_for_turn": R("RuntimeError", "boom")}),
]:
    add(F, {"config": cfg}, io)

# ------------------------------------------------------------------ _decide
F = "ToolCallGuardrailController._decide"
dec_cases = [
    ("warn", "repeated_exact_failure_warning", "read_file", 3, sig("read_file", "h"), None, {}, None),
    ("block", "repeated_exact_failure_block", "terminal", 5, sig("terminal", "h"), None, {}, None),
    ("halt", "identical_cycle_halt", "edit", 4, sig("edit", "h"), None, {"period": 2}, None),
    ("allow", "allow", "web_search", 0, sig("web_search", "h"), None, {}, None),
    ("warn", "idempotent_no_progress_warning", "search_files", 2, sig("search_files", "h"), "custom msg", {}, None),
    ("block", "loop_web_search_cap", "web_search", 50, sig("web_search", "h"), None, {"cap": 50}, None),
    ("halt", "identical_call_streak_halt", "process_manage", 5, sig("process_manage", "h"), None, {}, {"_halt_decision": None}),
]
for action, code, tn, count, signature, message, fmt, self_state in dec_cases:
    add(F, {"action": action, "code": code, "tool_name": tn, "count": count,
            "signature": signature, "message": message, "fmt": fmt},
        self_state=self_state or {})

# -------------------------------------------------------- _detect_identical_cycle
F = "ToolCallGuardrailController._detect_identical_cycle"
A = sig("edit", "ha")
B = sig("edit", "hb")
C = sig("edit", "hc")
D = sig("edit", "hd")
hist_cases = [
    [],
    [entry(A, "ra"), entry(B, "rb"), entry(A, "ra"), entry(B, "rb"),
     entry(A, "ra"), entry(B, "rb")],                       # period 2, 3 laps -> [2,3]
    [entry(A, "ra"), entry(B, "rb"), entry(C, "rc")] * 3,  # period 3, 3 laps -> [3,3]
    [entry(A, "ra", True), entry(B, "rb", True)] * 3,      # all repeatable -> None
    [entry(A, "ra"), entry(B, "rb"), entry(A, "ra"), entry(B, "rb"), entry(A, "ra")],  # too short -> None
    [entry(A, "ra"), entry(B, "rb"), entry(C, "rc"), entry(D, "rd")] * 3,  # period 4 -> [4,3]
    [entry(A, "ra"), entry(B, "rb")] * 3 + [entry(C, "rc")],  # break trailing lap -> different
]
for hist in hist_cases:
    add(F, {}, self_state={"_call_history": hist})

# ------------------------------------------------------- record_persisted_result
F = "ToolCallGuardrailController.record_persisted_result"
for cid, fp, st in [
    ("c1", "/tmp/a", {}),
    ("", "/tmp/a", {}),
    ("c1", "", {}),
    ("c1", "/tmp/b", {"_persisted_result_paths": {"c1": "/tmp/a"}}),
    ("c2", "/tmp/c", {"_persisted_result_paths": {"x": "/tmp/x"}}),
    ("c3", "/tmp/d", {"_persisted_result_paths": {}}),
]:
    add(F, {"tool_call_id": cid, "file_path": fp}, self_state=st)

# ---------------------------------------------------------------- observe_call
F = "ToolCallGuardrailController.observe_call"
LONG = "x" * 600

def obs_state(**kw):
    st = {
        "config": {"hard_stop_enabled": False, "no_progress_block_after": 3},
        "_identical_streak_sig": None,
        "_identical_streak_result_hash": "",
        "_identical_streak_count": 0,
        "_identical_streak_first_call_id": "",
        "_call_history": [],
        "_halt_decision": None,
        "_persisted_result_paths": {},
    }
    st.update(kw)
    return st

# 1. first call, fresh state -> no notice, streak initialized
add(F, {"tool_name": "read_file", "args": {"path": "/a"}, "result": "content", "tool_call_id": "c1", "failed": False},
    {"coerce_args": {"path": "/a"}, "signature_from_call": sig("read_file", "h1"), "result_hash": "ra"},
    obs_state())

# 2. third consecutive identical call -> identical_call_streak notice "3rd"
add(F, {"tool_name": "read_file", "args": {"path": "/a"}, "result": "content", "tool_call_id": "c3", "failed": False},
    {"coerce_args": {"path": "/a"}, "signature_from_call": sig("read_file", "h1"), "result_hash": "ra"},
    obs_state(_identical_streak_sig=sig("read_file", "h1"), _identical_streak_result_hash="ra",
              _identical_streak_count=2, _identical_streak_first_call_id="c1"))

# 3. repeatable tool (process_manage) -> no streak notice
add(F, {"tool_name": "process_manage", "args": {"action": "poll"}, "result": "idle", "tool_call_id": "p3", "failed": False},
    {"coerce_args": {"action": "poll"}, "signature_from_call": sig("process_manage", "hp"), "result_hash": "rp"},
    obs_state(_identical_streak_sig=sig("process_manage", "hp"), _identical_streak_result_hash="rp",
              _identical_streak_count=5))

# 4. hard-stop enabled -> streak halt decision recorded
add(F, {"tool_name": "edit", "args": {"p": 1}, "result": "same", "tool_call_id": "e4", "failed": False},
    {"coerce_args": {"p": 1}, "signature_from_call": sig("edit", "he"), "result_hash": "re"},
    obs_state(config={"hard_stop_enabled": True, "no_progress_block_after": 3},
              _identical_streak_sig=sig("edit", "he"), _identical_streak_result_hash="re",
              _identical_streak_count=3))

# 5. batch-cycle: history ends in A,B,A,B,A; append B -> cycle [2,3]
add(F, {"tool_name": "edit", "args": {"p": 2}, "result": "same", "tool_call_id": "e5", "failed": False},
    {"coerce_args": {"p": 2}, "signature_from_call": B, "result_hash": "rb"},
    obs_state(_call_history=[entry(A, "ra"), entry(B, "rb"), entry(A, "ra"), entry(B, "rb"), entry(A, "ra")]))

# 6. non-string result -> streak cleared, no result_hash seam, no cycle check
add(F, {"tool_name": "read_file", "args": {"path": "/a"}, "result": {"a": 1}, "tool_call_id": "c6", "failed": False},
    {"coerce_args": {"path": "/a"}, "signature_from_call": sig("read_file", "h1")},
    obs_state(_identical_streak_sig=sig("read_file", "h1"), _identical_streak_result_hash="ra",
              _identical_streak_count=3))

# 7. byte-identical long result -> stub seam fires
add(F, {"tool_name": "read_file", "args": {"path": "/long"}, "result": LONG, "tool_call_id": "c7", "failed": False},
    {"coerce_args": {"path": "/long"}, "signature_from_call": sig("read_file", "hl"), "result_hash": "rl",
     "build_result_reference_stub": "[hermes note: byte-identical stub]"},
    obs_state(_identical_streak_sig=sig("read_file", "hl"), _identical_streak_result_hash="rl",
              _identical_streak_count=2, _identical_streak_first_call_id="c1"))

# 8. failed=True suppresses stub
add(F, {"tool_name": "read_file", "args": {"path": "/long"}, "result": LONG, "tool_call_id": "c8", "failed": True},
    {"coerce_args": {"path": "/long"}, "signature_from_call": sig("read_file", "hl"), "result_hash": "rl"},
    obs_state(_identical_streak_sig=sig("read_file", "hl"), _identical_streak_result_hash="rl",
              _identical_streak_count=2))

# 9. seam raise propagates (BaseException trio not swallowed anywhere)
add(F, {"tool_name": "read_file", "args": {"path": "/a"}, "result": "content", "tool_call_id": "c9", "failed": False},
    {"coerce_args": R("SystemExit", "bye")}, obs_state())

# 10. fail-closed coerce_args (no script)
add(F, {"tool_name": "read_file", "args": {"path": "/a"}, "result": "content", "tool_call_id": "c10", "failed": False},
    {}, obs_state())


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %s: %d cases" % (OUT, len(CASES)))
    for fn in FUNCS:
        n = counts.get(fn, 0)
        print("  %-52s %d%s" % (fn, n, "" if n >= 5 else "  (<5!)"))
    assert all(counts.get(fn, 0) >= 5 for fn in FUNCS), "every function needs >=5 cases"
    assert set(counts) == set(FUNCS), "expected exactly the 11 unclaimed functions"


if __name__ == "__main__":
    main()
