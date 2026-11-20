#!/usr/bin/env python3
"""Generate tests/fixtures/conversation_loop_flow2.jsonl — scripted universal-seam
cases for the UNCLAIMED-remainder agent/conversation_loop.py FLOW defs ported in
src/agent/conversation_loop_flow2.c (_review_input_budget_exhausted,
_should_rearm_compression_budget, _billing_terminal_label, _billing_failure_result,
_LoopState, _run_conversation_turn).

Each case is {"func": <live_name>, "in": {...}} where in.io carries the seam
scripts (seam-name -> constant / per-call queue / {"__raise__":...} /
{"__script__":true,"ret":...}), in.state carries the mutable carriers, and the
remaining in.* keys are scalar / double inputs. >=5 cases per function cover the
decision branches, ordered-trace short-circuits, scripted raises, BaseException
passthrough, and fail-closed (missing-script) propagation.

Value model: attr doubles {"__attr__":{...}}; callable doubles {"__callable__":...};
the _LoopState dataclass projects as a plain field dict. ASCII text; integral
floats == ints. A bare JSON array script is a per-call QUEUE, so a list RESULT is
wrapped {"__script__":true,"ret":[...]}. Loop fixtures stay in the terminating
domain (scripted break/return verdicts).

Run: python tests/gen_conversation_loop_flow2_cases.py
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "conversation_loop_flow2.jsonl"


def attr(d, cls=None):
    o = {"__attr__": d}
    if cls:
        o["__class__"] = cls
    return o


def call(seam):
    return {"__callable__": seam}


def rs(kind, msg=""):
    return {"__raise__": kind, "message": msg}


def sc(ret):
    return {"__script__": True, "ret": ret}


CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


# ── 1. _review_input_budget_exhausted ─────────────────────────────────────────
def rv(budget, used, present_budget=True, present_used=True):
    d = {}
    if present_budget:
        d["_review_input_token_budget"] = budget
    if present_used:
        d["session_input_tokens"] = used
    return {"state": {"agent": attr(d)}}


case("_review_input_budget_exhausted", **rv(5, 5))            # used>=budget -> True
case("_review_input_budget_exhausted", **rv(5, 4))            # used<budget -> False
case("_review_input_budget_exhausted", **rv(5, 6))            # True
case("_review_input_budget_exhausted", **rv(0, 5))            # budget<=0 -> False
case("_review_input_budget_exhausted", **rv(-1, 5))           # budget<=0 -> False
case("_review_input_budget_exhausted", **rv(True, 5))         # bool budget -> False
case("_review_input_budget_exhausted", **rv(5, True))         # bool used -> False
case("_review_input_budget_exhausted", **rv("5", 5))          # str budget -> False
case("_review_input_budget_exhausted", **rv(5.5, 6))          # float budget -> False
case("_review_input_budget_exhausted", **rv(5, 5, present_budget=False))  # absent budget
case("_review_input_budget_exhausted", **rv(5, 0, present_used=False))    # absent used -> 0

# ── 2. _should_rearm_compression_budget ───────────────────────────────────────
def rearm(ca, ccp, pt, tt):
    return {"compression_attempts": ca, "completed_compaction_pending": ccp,
            "prompt_tokens": pt, "threshold_tokens": tt}


case("_should_rearm_compression_budget", **rearm(1, True, 5, 10))   # True
case("_should_rearm_compression_budget", **rearm(0, True, 5, 10))   # attempts 0
case("_should_rearm_compression_budget", **rearm(1, False, 5, 10))  # latch off
case("_should_rearm_compression_budget", **rearm(1, True, 0, 10))   # prompt 0
case("_should_rearm_compression_budget", **rearm(1, True, 5, 5))    # prompt==threshold
case("_should_rearm_compression_budget", **rearm(1, True, 10, 5))   # prompt>threshold
case("_should_rearm_compression_budget", **rearm(2, True, 9, 10))   # True

# ── 3. _billing_terminal_label ────────────────────────────────────────────────
case("_billing_terminal_label", summary="sum", unverified=False)
case("_billing_terminal_label", summary="sum", unverified=True)
case("_billing_terminal_label", summary="", unverified=False)
case("_billing_terminal_label", summary="err detail", unverified=True)
case("_billing_terminal_label", summary="x", unverified=False)

# ── 4. _billing_failure_result ────────────────────────────────────────────────
def classified(u=False, reason="billing", retry=False):
    return attr({"billing_unverified": u, "reason": attr({"value": reason}), "retryable": retry})


def bfr(**kw):
    base = {"classified": classified(), "summary": "S", "messages": [{"role": "user"}],
            "api_call_count": 2, "provider": "p", "base_url": "http://x", "model": "m"}
    base.update(kw)
    return base


case("_billing_failure_result", **bfr(io={"billing_or_entitlement_message": "G", "billing_block_dict": {"b": 1}}))
case("_billing_failure_result", **bfr(guidance="G", io={"billing_block_dict": {"b": 1}}))
case("_billing_failure_result", **bfr(usage_window=["WIN", 123.5, {"limit": 5}], io={"billing_block_dict": {"b": 1}}))
case("_billing_failure_result", **bfr(usage_window=[], io={"billing_or_entitlement_message": "G", "billing_block_dict": None}))
case("_billing_failure_result", **bfr(classified=classified(u=True), io={"billing_or_entitlement_message": "G", "billing_block_dict": {"b": 1}}))
case("_billing_failure_result", **bfr(usage_window=["WIN", None, {}], io={"billing_block_dict": {"b": 1}}))
case("_billing_failure_result", **bfr(usage_window=None))  # tuple(None) -> TypeError
case("_billing_failure_result", **bfr(io={"billing_or_entitlement_message": rs("ValueError", "bad")}))
case("_billing_failure_result", **bfr(io={"billing_or_entitlement_message": "G", "billing_block_dict": rs("SystemExit", "bye")}))
case("_billing_failure_result", **bfr(io={}))  # fail-closed guidance fetch
case("_billing_failure_result", **bfr(classified=attr({"reason": attr({"value": "billing"}), "retryable": False}), io={"billing_or_entitlement_message": "G", "billing_block_dict": {"b": 1}}))  # absent billing_unverified -> getattr default False
case("_billing_failure_result", **bfr(classified=attr({"billing_unverified": False, "reason": attr({"value": "billing"})}), io={"billing_or_entitlement_message": "G", "billing_block_dict": {"b": 1}}))  # absent retryable -> AttributeError

# ── 5. _LoopState ─────────────────────────────────────────────────────────────
def ctx_full(**over):
    d = {"user_message": "u", "original_user_message": "u", "conversation_history": [],
         "effective_task_id": "t", "turn_id": "T1", "should_review_memory": False,
         "plugin_user_context": None, "ext_prefetch_cache": None,
         "messages": [{"role": "user"}], "active_system_prompt": "SYS",
         "current_turn_user_idx": 0, "preflight_compression_blocked": False,
         "plugin_block_message": ""}
    d.update(over)
    return attr(d)


def ls_case(ctx, **kw):
    base = {"ctx": ctx, "system_message": "SYS", "moa_config": None, "max_compression_attempts": 3}
    base.update(kw)
    return base


case("_LoopState", **ls_case(ctx_full()))
case("_LoopState", **ls_case(ctx_full(turn_id="T2", current_turn_user_idx=4), max_compression_attempts=7))
case("_LoopState", **ls_case(ctx_full(active_system_prompt=None), moa_config={"cfg": 1}))
case("_LoopState", **ls_case(ctx_full(messages=[])))
case("_LoopState", **ls_case(attr({"user_message": "u"})))  # missing CTX fields -> AttributeError

# ── 6. _run_conversation_turn ─────────────────────────────────────────────────
def agent_double(**over):
    d = {"max_iterations": 5, "iteration_budget": attr({"remaining": 10}),
         "_budget_grace_call": False, "_api_max_retries": 3, "max_compression_attempts": 3}
    d.update(over)
    return attr(d)


def verdicts_happy(extra_norm=None, last="break"):
    norm = {"action": None}
    if extra_norm:
        norm = dict(norm, **extra_norm)
    return [
        {"action": None}, {"action": None}, {"action": None}, {"action": None},
        {"action": None}, {"action": None}, {"action": None},
        norm, {"action": last},
    ]


def turn_io(**over):
    io = {
        "decode_inline_moa_turn": sc(["u", None, None]),
        "begin_fast_mode_turn": None,
        "try_refresh_env_client_credentials": None,
        "held_stream_transforms": sc([]),
        "build_turn_context": ctx_full(),
        "begin_voice_turn_route": "SYS2",
        "turn_without_loop": None,
        "play_prelude": sc(["loop", None]),
        "run_phase": verdicts_happy(),
        "time": 1000.0,
        "turn_retry_state": attr({"x": 1}),
        "run_api_retry_loop": None,
        "finalize_from_state": {"final_response": "DONE", "completed": True},
    }
    io.update(over)
    return io


def turn_case(io, state_agent=None, **inp):
    base = {"user_message": "u", "state": {"agent": state_agent or agent_double()}, "io": io}
    base.update(inp)
    return base


# happy path: single iteration, finish_text_response break -> finalize
case("_run_conversation_turn", **turn_case(turn_io()))
# moa_config provided -> skip decode
case("_run_conversation_turn", **turn_case(turn_io(), moa_config={"cfg": 1}))
# decode wrong shape -> ValueError
case("_run_conversation_turn", **turn_case(turn_io(decode_inline_moa_turn=sc(["only", "two"]))))
# turn_without_loop early result
case("_run_conversation_turn", **turn_case(turn_io(turn_without_loop={"final_response": "BLOCKED", "completed": True})))
# play_prelude return
case("_run_conversation_turn", **turn_case(turn_io(play_prelude=sc(["return", {"final_response": "PRELUDE"}]))))
# preflight gate return
case("_run_conversation_turn", **turn_case(turn_io(run_phase=[
    {"action": None}, {"action": None}, {"action": None}, {"action": None},
    {"action": "return", "result": {"final_response": "GATE"}},
])))
# preflight gate continue then break (2 iterations, terminating)
case("_run_conversation_turn", **turn_case(turn_io(run_phase=[
    {"action": None}, {"action": None}, {"action": None}, {"action": None}, {"action": "continue"},
    {"action": None}, {"action": None}, {"action": None}, {"action": None}, {"action": "break"},
])))
# build_turn_context PreflightCompressionTimedOut -> preflight_timeout_result
case("_run_conversation_turn", **turn_case(turn_io(
    build_turn_context=rs("PreflightCompressionTimedOut", "too slow"),
    preflight_timeout_result={"final_response": "TIMEOUT", "failed": True})))
# try_refresh Exception -> logger.debug, then continue
case("_run_conversation_turn", **turn_case(turn_io(
    try_refresh_env_client_credentials=rs("ValueError", "env bad"), **{"logger.debug": None})))
# try_refresh BaseException -> propagate
case("_run_conversation_turn", **turn_case(turn_io(try_refresh_env_client_credentials=rs("SystemExit", "bye"))))
# normalize_model_response Exception -> handle_outer_loop_error break -> finalize
case("_run_conversation_turn", **turn_case(turn_io(run_phase=[
    {"action": None}, {"action": None}, {"action": None}, {"action": None},
    {"action": None}, {"action": None}, {"action": None},
    rs("ValueError", "normalize bad"),
    {"action": "break"},
])))
# run_api_retry_loop early result
case("_run_conversation_turn", **turn_case(turn_io(run_api_retry_loop={"final_response": "API_RESULT"})))
# compression_timeout_exhausted -> result.update
case("_run_conversation_turn", **turn_case(turn_io(run_phase=verdicts_happy(
    extra_norm={"_compression_timeout_exhausted": True}))))
# run_tool_round branch (tool_calls non-empty)
case("_run_conversation_turn", **turn_case(turn_io(run_phase=[
    {"action": None}, {"action": None}, {"action": None}, {"action": None},
    {"action": None}, {"action": None}, {"action": None},
    {"action": None, "assistant_message": attr({"tool_calls": [{"id": "c1"}]})},
    {"action": "break"},
])))
# fail-closed: missing begin_fast_mode_turn script
case("_run_conversation_turn", **turn_case({k: v for k, v in turn_io().items() if k != "begin_fast_mode_turn"}))
# held transforms non-empty -> _hold_stream True
case("_run_conversation_turn", **turn_case(turn_io(held_stream_transforms=sc([{"t": 1}]))))
# budget grace call enters loop even when remaining 0
case("_run_conversation_turn", **turn_case(turn_io(),
     state_agent=agent_double(iteration_budget=attr({"remaining": 0}), _budget_grace_call=True)))

# ── write ─────────────────────────────────────────────────────────────────────
OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")

# per-function count check (>=5 each)
from collections import Counter
cnt = Counter(c["func"] for c in CASES)
print("wrote %d cases to %s" % (len(CASES), OUT))
for fn, n in sorted(cnt.items()):
    print("  %-35s %d%s" % (fn, n, "" if n >= 5 else "  (<5!)"))
assert all(n >= 5 for n in cnt.values()), "a function has <5 cases"