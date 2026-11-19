#!/usr/bin/env python3
"""Generator for tests/fixtures/agent_runtime_helpers_flow2.jsonl.

Emits >=5 cases per ported function of agent/agent_runtime_helpers.py (the
UNCLAIMED remainder ported as src/agent/agent_runtime_helpers_flow2.c). Each case
is {"func": <name>, "in": {...args..., "io": {seam: script}, "env": {...}}}.

Script shapes (shared with the C universal seam arh2_call and the oracle):
  - absent key            -> None
  - {"$seq": [...]}       -> per-call queue (exhaustion raises RuntimeError)
  - {"__raise__": cls, "msg": m} -> raises a REAL exception of that class
  - any other JSON value  -> reused for every call

Domain restrictions (documented in the port header):
  - ASCII str operands; .strip() ASCII whitespace.
  - tool_call entries are dicts with dict-or-null function; function.name /
    tool_call_id are str-or-null; variants scripts are arrays of strings.
  - object-valued seam results consumed by `if obj:` truthiness (pool.current)
    are null or a NON-EMPTY record.
  - json.dumps content is ASCII scalars/containers (no non-integral floats).
  - integral floats collapse to ints on both sides.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "agent_runtime_helpers_flow2.jsonl")

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def seq(*vals):
    return {"$seq": list(vals)}


def raise_(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def tc(id=None, name=None, call_id=None, args="{}"):
    d = {"type": "function", "function": {"name": name, "arguments": args}}
    if id is not None:
        d["id"] = id
    if call_id is not None:
        d["call_id"] = call_id
    return d


def assistant(*tcs, **extra):
    d = {"role": "assistant", "tool_calls": list(tcs)}
    d.update(extra)
    return d


def tool(tid, name=None, content="ok"):
    d = {"role": "tool", "tool_call_id": tid, "content": content}
    if name is not None:
        d["name"] = name
    return d


def user(content="hi"):
    return {"role": "user", "content": content}


# ── _prepend_corruption_marker ──────────────────────────────────────────────
M = "MARK"
case("_prepend_corruption_marker", tool_msg={"role": "tool", "content": "result", "_db_persisted": True}, marker=M)
case("_prepend_corruption_marker", tool_msg={"role": "tool", "content": M + " already", "_db_persisted": True}, marker=M)
case("_prepend_corruption_marker", tool_msg={"role": "tool", "content": None, "_db_persisted": True}, marker=M)
case("_prepend_corruption_marker", tool_msg={"role": "tool", "content": ""}, marker=M)
case("_prepend_corruption_marker", tool_msg={"role": "tool", "content": {"a": 1, "b": [2, "x"]}}, marker=M)
case("_prepend_corruption_marker", tool_msg={"role": "tool", "content": [1, "x", True, None]}, marker=M)
case("_prepend_corruption_marker", tool_msg={"role": "tool", "content": "tail"}, marker=M,
     env={"db_persisted_marker": "_custom_mark"})

# ── _find_tool_result ───────────────────────────────────────────────────────
case("_find_tool_result", messages=[tool("c1")], start=0, tool_call={"id": "c1"},
     io={"tool_result_id_variants": ["c1"], "tool_call_id_variants": ["c1"]})
case("_find_tool_result", messages=[user(), tool("c1")], start=0, tool_call={"id": "c1"},
     io={"tool_result_id_variants": ["c1"], "tool_call_id_variants": ["c1"]})
case("_find_tool_result", messages=[tool("x")], start=0, tool_call={"id": "c1"},
     io={"tool_result_id_variants": ["x"], "tool_call_id_variants": ["c1"]})
case("_find_tool_result", messages=[tool("c0"), tool("c1")], start=1, tool_call={"id": "c1"},
     io={"tool_result_id_variants": ["c1"], "tool_call_id_variants": ["c1"]})
case("_find_tool_result", messages=["not-a-dict"], start=0, tool_call={"id": "c1"},
     io={"tool_result_id_variants": ["c1"], "tool_call_id_variants": ["c1"]})
case("_find_tool_result", messages=[tool("x"), tool("c1")], start=0, tool_call={"id": "c1"},
     io={"tool_result_id_variants": seq(["x"], ["c1"]), "tool_call_id_variants": ["c1"]})
case("_find_tool_result", messages=[tool("c1")], start=0, tool_call={"id": "c1"},
     io={"tool_result_id_variants": raise_("ValueError", "bad variant")})

# ── _drop_invalid_roles ─────────────────────────────────────────────────────
case("_drop_invalid_roles", messages=[user(), assistant(), tool("c1")])
case("_drop_invalid_roles", messages=[user(), {"role": "bogus", "content": "x"}],
     io={"ra.logger.debug": None})
case("_drop_invalid_roles", messages=[{"role": None}, {"role": "user"}], io={"ra.logger.debug": None})
case("_drop_invalid_roles", messages=[{"role": 5}, {"role": "tool", "tool_call_id": "c1"}],
     io={"ra.logger.debug": None})
case("_drop_invalid_roles", messages=[assistant(), user()], env={"valid_api_roles": ["user"]},
     io={"ra.logger.debug": None})
case("_drop_invalid_roles", messages=[{"role": "bogus"}, {"role": "also_bad"}], io={"ra.logger.debug": None})

# ── _drop_empty_tool_calls_arrays ───────────────────────────────────────────
case("_drop_empty_tool_calls_arrays", messages=[assistant(content="hi")], io={"ra.logger.debug": None})
case("_drop_empty_tool_calls_arrays", messages=[assistant(tc("c1", "f"))])
case("_drop_empty_tool_calls_arrays", messages=[{"role": "assistant", "tool_calls": "x"}],
     io={"ra.logger.debug": None})
case("_drop_empty_tool_calls_arrays", messages=[{"role": "assistant", "tool_calls": None}],
     io={"ra.logger.debug": None})
case("_drop_empty_tool_calls_arrays", messages=[user(), tool("c1")])
case("_drop_empty_tool_calls_arrays", messages=[{"role": "tool", "tool_calls": []}])
case("_drop_empty_tool_calls_arrays", messages=[assistant(content="a"), assistant(content="b")],
     io={"ra.logger.debug": None})

# ── _repair_invalid_tool_call_names ─────────────────────────────────────────
case("_repair_invalid_tool_call_names", messages=[assistant(tc("c1", "read_file"))],
     io={"coerce_tool_name": "read_file"})
case("_repair_invalid_tool_call_names", messages=[assistant(tc("c1", "multi_tool_use.parallel"))],
     io={"coerce_tool_name": "invalid_tool_call", "ra._get_tool_call_id_static": "c1", "ra.logger.warning": None})
case("_repair_invalid_tool_call_names", messages=[assistant({"id": "c1", "function": None})],
     io={"coerce_tool_name": "invalid_tool_call", "ra._get_tool_call_id_static": "c1", "ra.logger.warning": None})
case("_repair_invalid_tool_call_names", messages=[assistant({"id": "c1", "function": {"name": None}})],
     io={"coerce_tool_name": "invalid_tool_call", "ra._get_tool_call_id_static": "c1", "ra.logger.warning": None})
case("_repair_invalid_tool_call_names", messages=[assistant(tc("c1", "read_file"), tc("c2", "bad name"))],
     io={"coerce_tool_name": seq("read_file", "invalid_tool_call"),
         "ra._get_tool_call_id_static": "c2", "ra.logger.warning": None})
case("_repair_invalid_tool_call_names", messages=[assistant(tc("c1", "x"))],
     io={"coerce_tool_name": raise_("ValueError", "coerce fail")})
case("_repair_invalid_tool_call_names", messages=[user(), tool("c1")], io={"coerce_tool_name": "x"})

# ── _drop_results_without_ids ───────────────────────────────────────────────
case("_drop_results_without_ids", messages=[tool("c1"), tool(""), tool(None)], io={"ra.logger.debug": None})
case("_drop_results_without_ids", messages=[tool("c1"), tool("c2")])
case("_drop_results_without_ids", messages=[tool("   ")], io={"ra.logger.debug": None})
case("_drop_results_without_ids", messages=[user(), assistant()])
case("_drop_results_without_ids", messages=[tool(""), tool(None), tool("c1")], io={"ra.logger.debug": None})
case("_drop_results_without_ids", messages=[{"role": "tool"}], io={"ra.logger.debug": None})

# ── _pair_tool_calls_positionally ───────────────────────────────────────────
case("_pair_tool_calls_positionally", messages=[assistant(tc("c1", "f")), tool("c1")],
     io={"tool_call_id_variants": ["c1"], "tool_result_id_variants": ["c1"]})
case("_pair_tool_calls_positionally", messages=[assistant(tc("c1", "f"))],
     io={"tool_call_id_variants": ["c1"], "ra._get_tool_call_name_static": "read_file",
         "coalesce_tool_call_id": "c1", "ra.logger.debug": None})
case("_pair_tool_calls_positionally", messages=[tool("c1"), assistant(tc("c1", "f"))],
     io={"tool_result_id_variants": ["c1"], "tool_call_id_variants": ["c1"],
         "ra._get_tool_call_name_static": "read_file", "coalesce_tool_call_id": "c1", "ra.logger.debug": None})
case("_pair_tool_calls_positionally", messages=[assistant(tc("c1", "f1"), tc("c2", "f2")), tool("c1")],
     io={"tool_call_id_variants": seq(["c1"], ["c2"]), "tool_result_id_variants": ["c1"],
         "ra._get_tool_call_name_static": "f2", "coalesce_tool_call_id": "c2", "ra.logger.debug": None})
case("_pair_tool_calls_positionally", messages=[assistant(tc("c1", "f")), user(), tool("c1")],
     io={"tool_call_id_variants": ["c1"], "tool_result_id_variants": ["c1"],
         "ra._get_tool_call_name_static": "f", "coalesce_tool_call_id": "c1", "ra.logger.debug": None})
case("_pair_tool_calls_positionally", messages=[assistant(tc("c1", "f")), tool("c1"), tool("c1")],
     io={"tool_call_id_variants": ["c1"], "tool_result_id_variants": ["c1"], "ra.logger.debug": None})
case("_pair_tool_calls_positionally", messages=[user(), tool("c1")],
     io={"tool_result_id_variants": ["c1"], "ra.logger.debug": None})

# ── _dedupe_tool_call_ids ───────────────────────────────────────────────────
case("_dedupe_tool_call_ids", messages=[assistant(tc("c1", "f"), tc("c1", "g"))],
     io={"tool_call_id_variants": ["c1"], "ra.logger.debug": None})
case("_dedupe_tool_call_ids", messages=[assistant(tc("c1", "f")), tool("c1"), tool("c1")],
     io={"tool_call_id_variants": ["c1"], "tool_result_id_variants": ["c1"], "ra.logger.debug": None})
case("_dedupe_tool_call_ids", messages=[assistant(tc("c1", "f")), tool("c1"), assistant(tc("c1", "g")), tool("c1")],
     io={"tool_call_id_variants": ["c1"], "tool_result_id_variants": ["c1"]})
case("_dedupe_tool_call_ids", messages=[tool("c1")],
     io={"tool_result_id_variants": ["c1"], "ra.logger.debug": None})
case("_dedupe_tool_call_ids", messages=[assistant(tc("c1", "f")), tool("c1")],
     io={"tool_call_id_variants": ["c1"], "tool_result_id_variants": ["c1"]})
case("_dedupe_tool_call_ids", messages=[assistant(tc("c1", "f"), tc("c2", "g")), tool("c2"), tool("c1")],
     io={"tool_call_id_variants": seq(["c1"], ["c2"]), "tool_result_id_variants": seq(["c2"], ["c1"])})

# ── _realign_tool_result_names ──────────────────────────────────────────────
case("_realign_tool_result_names", messages=[assistant(tc("c1", "tool_call")), tool("c1", "mcp__x")],
     io={"ra._get_tool_call_id_static": "c1", "ra._get_tool_call_name_static": "tool_call", "ra.logger.debug": None})
case("_realign_tool_result_names", messages=[assistant(tc("c1", "tool_call")), tool("c1", "tool_call")],
     io={"ra._get_tool_call_id_static": "c1", "ra._get_tool_call_name_static": "tool_call"})
case("_realign_tool_result_names", messages=[assistant(tc("c1", "tool_call")), tool("c1")],
     io={"ra._get_tool_call_id_static": "c1", "ra._get_tool_call_name_static": "tool_call"})
case("_realign_tool_result_names", messages=[assistant(tc("c1", "tool_call")), tool(" c1 ", "mcp__x")],
     io={"ra._get_tool_call_id_static": " c1 ", "ra._get_tool_call_name_static": "tool_call", "ra.logger.debug": None})
case("_realign_tool_result_names",
     messages=[assistant(tc("c1", "tool_call"), tc("c2", "tool_call")), tool("c1", "a"), tool("c2", "b")],
     io={"ra._get_tool_call_id_static": seq("c1", "c2"), "ra._get_tool_call_name_static": "tool_call",
         "ra.logger.debug": None})
case("_realign_tool_result_names", messages=[user(), tool("c1", "x")],
     io={"ra._get_tool_call_id_static": "c1", "ra._get_tool_call_name_static": "f"})

# ── tool_results_this_turn ──────────────────────────────────────────────────
case("tool_results_this_turn", messages=[user(), tool("c1"), tool("c2")])
case("tool_results_this_turn", messages=[tool("c0"), user(), tool("c1")])
case("tool_results_this_turn", messages=[tool("c1"), tool("c2")])
case("tool_results_this_turn", messages=[])
case("tool_results_this_turn", messages=[user()])
case("tool_results_this_turn", messages=[user(), "skip", tool("c1")])
case("tool_results_this_turn", messages=None)

# ── _api_error_debug_info ───────────────────────────────────────────────────
case("_api_error_debug_info", error={"cls": "ValueError", "msg": "bad", "attrs": {"status_code": 400, "request_id": "r1"}})
case("_api_error_debug_info", error={"cls": "APIError", "msg": "e", "attrs": {"type": "err_type", "code": "c"}})
case("_api_error_debug_info", error={"cls": "HTTPError", "msg": "e", "attrs": {},
                                     "response": {"status_code": 500, "text": "body"}})
case("_api_error_debug_info", error={"cls": "HTTPError", "msg": "e", "attrs": {}, "response": {"status_code": 500}},
     io={"ra.logger.debug": None})
case("_api_error_debug_info", error={"cls": "ValueError", "msg": "e", "attrs": {"status_code": None, "param": "p"}})
case("_api_error_debug_info", error={"cls": "ValueError", "msg": "e", "attrs": {}, "response": None})
case("_api_error_debug_info", error={"cls": "Err", "msg": "e", "attrs": {"body": {"k": 1}, "request_id": "r"}})

# ── sync_credential_pool_entry_id ───────────────────────────────────────────
case("sync_credential_pool_entry_id", agent={"_credential_pool": {}, "api_key": "k"},
     io={"pool.entry_id_for_api_key": "e1"})
case("sync_credential_pool_entry_id", agent={"_credential_pool": None, "api_key": "k"})
case("sync_credential_pool_entry_id", agent={"api_key": "k"})
case("sync_credential_pool_entry_id", agent={"_credential_pool": {}, "api_key": "k"},
     io={"pool.entry_id_for_api_key": raise_("ValueError", "refresh fail")})
case("sync_credential_pool_entry_id", agent={"_credential_pool": {}, "api_key": "k"},
     io={"pool.entry_id_for_api_key": raise_("SystemExit", "bye")})
case("sync_credential_pool_entry_id", agent={"_credential_pool": {}},
     io={"pool.entry_id_for_api_key": "e2"})

# ── _failed_credential_identity ─────────────────────────────────────────────
case("_failed_credential_identity", agent={"api_key": "k", "_credential_pool_entry_id": "e1"}, pool={})
case("_failed_credential_identity", agent={"api_key": None, "_credential_pool_entry_id": None}, pool={},
     io={"pool.current": {"runtime_api_key": "k2", "id": "e2"}})
case("_failed_credential_identity", agent={"api_key": None, "_credential_pool_entry_id": "e1"}, pool={},
     io={"pool.current": {"runtime_api_key": "k2"}})
case("_failed_credential_identity", agent={"api_key": None}, pool={}, io={"pool.current": None})
case("_failed_credential_identity", agent={"api_key": None}, pool=None)
case("_failed_credential_identity", agent={"api_key": "k", "_credential_pool_entry_id": 5}, pool={})
case("_failed_credential_identity", agent={}, pool={}, io={"pool.current": {"id": "e3"}})

# ── _restore_runtime_capabilities ───────────────────────────────────────────
case("_restore_runtime_capabilities", agent={}, rt={"runtime_capabilities": {"a": 1}})
case("_restore_runtime_capabilities", agent={}, rt={"capabilities": {"b": 2}})
case("_restore_runtime_capabilities", agent={}, rt={"runtime_capabilities": None}, io={"logger.warning": None})
case("_restore_runtime_capabilities", agent={}, rt={"runtime_capabilities": "x"}, io={"logger.warning": None})
case("_restore_runtime_capabilities", agent={}, rt={})
case("_restore_runtime_capabilities", agent={"runtime_capabilities": {"old": 1}},
     rt={"runtime_capabilities": {}, "capabilities": {"z": 9}})

# ── _apply_primary_runtime_fields ───────────────────────────────────────────
RT = {"model": "m", "provider": "openai", "base_url": "https://x", "api_key": "k",
      "api_mode": "chat_completions", "client_kwargs": {"timeout": 30}}
case("_apply_primary_runtime_fields", agent={}, rt=dict(RT), io={"is_actual_route": False})
case("_apply_primary_runtime_fields", agent={}, rt=dict(RT), io={"is_actual_route": True})
case("_apply_primary_runtime_fields", agent={}, rt={"provider": "p"})
case("_apply_primary_runtime_fields", agent={}, rt=dict(RT, requested_provider="rp"), io={"is_actual_route": False})
case("_apply_primary_runtime_fields", agent={"_transport_cache": {"x": 1}}, rt=dict(RT), io={"is_actual_route": False})
case("_apply_primary_runtime_fields", agent={}, rt=dict(RT, request_overrides=None, reasoning_echo_flag=True),
     io={"is_actual_route": False})
case("_apply_primary_runtime_fields", agent={}, rt=dict(RT), io={"is_actual_route": raise_("OSError", "net")})

# ── _revert_credential_rotation ─────────────────────────────────────────────
case("_revert_credential_rotation", agent={"_credential_pool_revert_id": None})
case("_revert_credential_rotation", agent={"_credential_pool_revert_id": "e1", "_credential_pool": None})
case("_revert_credential_rotation", agent={"_credential_pool_revert_id": "e1", "_credential_pool": {},
                                           "_credential_pool_entry_id": "e1"})
case("_revert_credential_rotation",
     agent={"_credential_pool_revert_id": "e1", "_credential_pool": {}, "model": "m", "_swap_credential": {}},
     io={"pool.reclaim": {"id": "e1", "label": "L"}, "agent._swap_credential": None, "logger.info": None})
case("_revert_credential_rotation",
     agent={"_credential_pool_revert_id": "e1", "_credential_pool": {}, "model": "m", "_swap_credential": {}},
     io={"pool.reclaim": None})
case("_revert_credential_rotation",
     agent={"_credential_pool_revert_id": "e1", "_credential_pool": {}, "model": "m", "_swap_credential": {}},
     io={"pool.reclaim": raise_("ValueError", "reclaim fail"), "logger.warning": None})
case("_revert_credential_rotation",
     agent={"_credential_pool_revert_id": "e1", "_credential_pool": {}, "model": "m", "_swap_credential": {}},
     io={"pool.reclaim": {"id": "e1", "label": "L"}, "agent._swap_credential": False})
case("_revert_credential_rotation",
     agent={"_credential_pool_revert_id": "e1", "_credential_pool": {}, "model": "m"},
     io={"pool.reclaim": {"id": "e1", "label": "L"}})

# ── _requeue_pending_steer ──────────────────────────────────────────────────
case("_requeue_pending_steer", agent={"session_id": "s"}, steer_text="do x", io={"requeue_drained_steer": None})
case("_requeue_pending_steer", agent={}, steer_text="", io={"requeue_drained_steer": None})
case("_requeue_pending_steer", agent={}, steer_text="t", io={"requeue_drained_steer": raise_("ValueError", "no")})
case("_requeue_pending_steer", agent={"a": 1}, steer_text="hello", io={"requeue_drained_steer": None})
case("_requeue_pending_steer", agent={}, steer_text="multi word steer", io={"requeue_drained_steer": None})

# ── _pre_tool_block_message ─────────────────────────────────────────────────
case("_pre_tool_block_message", agent={"session_id": "s"}, function_name="f", function_args={"x": 1},
     effective_task_id="t", tool_call_id="c", middleware_trace=[],
     io={"_dispatch_pre_tool_call_hooks": ["block msg", {"x": 2}]})
case("_pre_tool_block_message", agent={}, function_name="f", function_args={"x": 1},
     effective_task_id=None, tool_call_id=None, middleware_trace=[],
     io={"_dispatch_pre_tool_call_hooks": [None, None]})
case("_pre_tool_block_message", agent={}, function_name="f", function_args={"x": 1},
     effective_task_id="t", tool_call_id="c", middleware_trace=None)
case("_pre_tool_block_message", agent={}, function_name="f", function_args={"x": 1},
     effective_task_id="t", tool_call_id="c", middleware_trace=[],
     io={"_dispatch_pre_tool_call_hooks": raise_("ValueError", "hook fail")})
case("_pre_tool_block_message", agent={}, function_name="f", function_args={"x": 1},
     effective_task_id="t", tool_call_id="c", middleware_trace=[],
     io={"_dispatch_pre_tool_call_hooks": raise_("SystemExit", "bye")})
case("_pre_tool_block_message", agent={"session_id": "s", "_current_turn_id": "tu", "_current_api_request_id": "ar"},
     function_name="f", function_args={}, effective_task_id="t", tool_call_id="c", middleware_trace=[{"m": 1}],
     io={"_dispatch_pre_tool_call_hooks": [None, {"y": 9}]})

# ── _snapshot_switch_state ──────────────────────────────────────────────────
case("_snapshot_switch_state", agent={})
case("_snapshot_switch_state", agent={"model": "m", "api_key": "k"})
case("_snapshot_switch_state", agent={"_client_kwargs": {"a": 1}})
case("_snapshot_switch_state", agent={"_client_kwargs": None})
case("_snapshot_switch_state", agent={"model": "m", "provider": "p", "base_url": "u", "_reasoning_echo_flag": True})
case("_snapshot_switch_state", agent={"client": None, "_anthropic_client": {"c": 1}})

# ── _restore_switch_snapshot ────────────────────────────────────────────────
case("_restore_switch_snapshot", agent={}, snapshot={"model": "m", "provider": {"__missing__": True}})
case("_restore_switch_snapshot", agent={}, snapshot={})
case("_restore_switch_snapshot", agent={}, snapshot={"api_key": None})
case("_restore_switch_snapshot", agent={"model": "old"}, snapshot={"model": "new", "api_mode": "chat_completions"})
case("_restore_switch_snapshot", agent={}, snapshot={"model": "m", "base_url": "u", "_client_kwargs": {"a": 1}})
case("_restore_switch_snapshot", agent={"provider": "p"}, snapshot={"provider": {"__missing__": True}, "model": "m"})

# ── _recover_auth_failure ───────────────────────────────────────────────────
case("_recover_auth_failure", agent={"a": 1}, pool={"p": 1}, kwargs={"status_code": 401},
     io={"recover_auth_failure": {"ok": True}})
case("_recover_auth_failure", agent={}, pool={}, kwargs={}, io={"recover_auth_failure": None})
case("_recover_auth_failure", agent=None, pool=None, kwargs={"x": 1}, io={"recover_auth_failure": "r"})
case("_recover_auth_failure", agent={}, pool={}, kwargs={}, io={"recover_auth_failure": raise_("ValueError", "auth")})
case("_recover_auth_failure", agent={"id": "a"}, pool={"id": "p"}, kwargs={"force": True, "n": 2},
     io={"recover_auth_failure": [True, False]})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-function coverage report
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for fn in sorted(counts):
        flag = "" if counts[fn] >= 5 else "  <-- UNDER 5"
        print(f"  {fn:42s} {counts[fn]}{flag}")
    under = [fn for fn, n in counts.items() if n < 5]
    if under:
        raise SystemExit(f"UNDER 5 cases: {under}")


if __name__ == "__main__":
    main()