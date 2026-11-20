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



# ══ UNCLAIMED remainder cases (27 fns) ═════════════════════════════════════

def envx(**kw):
    d = {"now": 100.0, "monotonic": 1000.0, "codex_quota_probe_min_interval": 30.0,
         "status_exhausted": "exhausted",
         "usage_limit_reason_tokens": ["usage_limit_reached", "gousagelimit"],
         "usage_limit_message_tokens": ["usage limit reached", "usage limit has been reached"],
         "degenerate_final_max_chars": 24,
         "sentence_terminals": [".", "!", "?"],
         "degenerate_leading_punct": "?!,;:)]}",
         "providers_import_ok": True}
    d.update(kw)
    return d


def oid(i, **kw):
    d = {"__id": i}
    d.update(kw)
    return d


# ── _cursor_skip_prefix ──
case("_cursor_skip_prefix", messages=[oid(1), oid(2)], cursor={"prefix": [oid(1)]})
case("_cursor_skip_prefix", messages=[oid(1)], cursor=None)
case("_cursor_skip_prefix", messages=[oid(1), oid(2)], cursor={"prefix": [oid(1), oid(3)]})
case("_cursor_skip_prefix", messages=[oid(1)], cursor={"prefix": []})
case("_cursor_skip_prefix", messages=[], cursor={"prefix": [oid(1)]})
case("_cursor_skip_prefix", messages=[oid(1), oid(2)], cursor={"prefix": "notlist"})

# ── _recover_rate_limit ──
RLC = "rate limit"
case("_recover_rate_limit", credential_id="c1", has_retried_429=False, api_key_hint=None, error_context=None,
     io={"pool.entries": [{"id": "c1", "last_status": "exhausted"}], "ra.logger.info": None,
         "rotate_and_swap": True}, env=envx())
case("_recover_rate_limit", credential_id=None, has_retried_429=False, api_key_hint=None, error_context=None,
     io={"pool.current": {"last_status": "ok"}}, env=envx())
case("_recover_rate_limit", credential_id=None, has_retried_429=True, api_key_hint=None, error_context=None,
     io={"pool.current": {"last_status": "ok"}, "rotate_and_swap": False}, env=envx())
case("_recover_rate_limit", credential_id=None, has_retried_429=True, error_context={"reason": "UsageLimitReached"},
     io={"pool.current": {"last_status": "ok"}, "rotate_and_swap": True}, env=envx())
case("_recover_rate_limit", credential_id=None, api_key_hint="sk-hint", has_retried_429=False, error_context=None,
     io={"pool.entries": [{"id": "c1", "runtime_api_key": "sk-hint", "last_status": "exhausted"}],
         "ra.logger.info": None, "rotate_and_swap": True}, env=envx())
case("_recover_rate_limit", credential_id="missing", api_key_hint=None, has_retried_429=False, error_context=None,
     io={"pool.entries": [{"id": "c1", "last_status": "ok"}], "pool.current": {"last_status": "ok"}}, env=envx())

# ── _build_anthropic_client_from_runtime ──
RT = {"anthropic_api_key": "sk-a", "anthropic_base_url": "https://api.anthropic.com", "is_anthropic_oauth": False}
for _p, _m in [("anthropic", "claude"), ("custom", "m"), ("bedrock", "b"), ("", ""), ("openai", "gpt")]:
    case("_build_anthropic_client_from_runtime", agent={"provider": _p, "model": _m}, rt=RT,
         io={"get_provider_request_timeout": 30, "build_anthropic_client": {"id": "cli"}})

# ── _rebuild_primary_client ──
for _prov in ["moa", " MOA ", "bedrock", "openai", "anthropic"]:
    _ag = {"provider": _prov, "model": "m", "api_mode": "anthropic_messages" if _prov == "bedrock" else "chat_completions",
           "base_url": "https://x"}
    case("_rebuild_primary_client", agent=_ag, rt={"client_kwargs": {"api_key": "k"}}, reason="restore",
         io={"build_moa_facade": {"id": "moa"}, "bind_bedrock_runtime": None,
             "get_provider_request_timeout": 30, "build_anthropic_client": {"id": "cli"},
             "agent._create_openai_client": {"id": "oai"}})

# ── _primary_reset_gate_blocks ──
PRG = {"model": "primary-model"}
case("_primary_reset_gate_blocks", agent={}, rt=PRG, primary_provider="openai", primary_runtime_base_url="",
     io={"matches_primary": True, "pool.next_available_at": None}, env=envx())
case("_primary_reset_gate_blocks", agent={}, rt=PRG, primary_provider="openai", primary_runtime_base_url="",
     io={"matches_primary": False, "load_primary_pool": {"id": "p"}, "pool.next_available_at": None}, env=envx())
case("_primary_reset_gate_blocks", agent={}, rt=PRG, primary_provider="openai", primary_runtime_base_url="",
     io={"matches_primary": True, "pool.next_available_at": 200, "datetime.isoformat": "t=200",
         "logger.info": None}, env=envx())
case("_primary_reset_gate_blocks", agent={"_restore_wait_logged": True}, rt=PRG, primary_provider="openai",
     primary_runtime_base_url="", io={"matches_primary": True, "pool.next_available_at": 200}, env=envx())
case("_primary_reset_gate_blocks", agent={}, rt={}, primary_provider="bedrock", primary_runtime_base_url="",
     io={"matches_primary": True, "pool.next_available_at": 50}, env=envx())

# ── _rebind_primary_credential_pool ──
case("_rebind_primary_credential_pool", agent={}, primary_provider="openai", primary_model="m", prefetched=False,
     io={"matches_primary": False}, env=envx())
case("_rebind_primary_credential_pool", agent={"_credential_pool": {"provider": "bedrock"}},
     primary_provider="openai", primary_model="m", prefetched=False,
     io={"matches_primary": False, "load_primary_pool": {"provider": "openai", "has_available": True},
         "pool.has_available": False}, env=envx())
case("_rebind_primary_credential_pool", agent={"_credential_pool": {"provider": "bedrock"}},
     primary_provider="openai", primary_model="m", prefetched=False,
     io={"matches_primary": False, "load_primary_pool": {"provider": "openai"},
         "pool.has_available": True, "pool.select": {"id": "e1", "runtime_api_key": "sk", "provider": "openai"},
         "agent._swap_credential": None, "logger.info": None}, env=envx())
case("_rebind_primary_credential_pool", agent={"_credential_pool": {"provider": "openai"}},
     primary_provider="openai", primary_model="m", prefetched=False,
     io={"matches_primary": True, "pool.has_available": True,
         "pool.select": {"id": "e1", "access_token": "tok", "provider": "openai"},
         "agent._swap_credential": None, "logger.info": None}, env=envx())
case("_rebind_primary_credential_pool", agent={"_credential_pool": {"provider": "bedrock"}},
     primary_provider="openai", primary_model="m", prefetched=False,
     io={"matches_primary": False, "load_primary_pool": {"provider": "openai"},
         "pool.has_available": True, "pool.select": {"id": "e1", "runtime_api_key": "sk", "provider": "bedrock"},
         "logger.info": None}, env=envx())

# ── _primary_quota_reopened_early ──
case("_primary_quota_reopened_early", agent={}, primary_provider="openai", primary_model="m", env=envx())
case("_primary_quota_reopened_early", agent={"_codex_reopen_checked_at": 995}, primary_provider="openai-codex",
     primary_model="m", env=envx())
case("_primary_quota_reopened_early", agent={}, primary_provider="openai-codex", primary_model="m",
     io={"matches_primary": True, "load_primary_pool": None}, env=envx())
case("_primary_quota_reopened_early", agent={}, primary_provider="openai-codex", primary_model="m",
     io={"matches_primary": True, "load_primary_pool": {"id": "p"}, "pool.next_available_at": 50}, env=envx())
case("_primary_quota_reopened_early", agent={}, primary_provider="openai-codex", primary_model="m",
     io={"matches_primary": True, "load_primary_pool": {"id": "p"}, "pool.next_available_at": 200,
         "pool.lift_reopened_cooldowns": True}, env=envx())

# ── _moa_aggregator_cache_policy ──
case("_moa_aggregator_cache_policy", agent={}, eff_model="m",
     io={"load_config": {"moa": {}}, "resolve_moa_preset": {}})
case("_moa_aggregator_cache_policy", agent={}, eff_model="m",
     io={"load_config": {"moa": {}}, "resolve_moa_preset": {"aggregator": {"provider": "anthropic", "model": "c"}},
         "resolve_runtime_provider": {"base_url": "https://a", "api_mode": "anthropic_messages"},
         "anthropic_prompt_cache_policy": [True, False]})
case("_moa_aggregator_cache_policy", agent={}, eff_model="m",
     io={"load_config": {"moa": {}}, "resolve_moa_preset": {"aggregator": {"provider": "x", "model": "y"}},
         "resolve_runtime_provider": raise_("ValueError", "no rt"), "anthropic_prompt_cache_policy": [False, True]})
case("_moa_aggregator_cache_policy", agent={}, eff_model="m",
     io={"load_config": raise_("RuntimeError", "cfg"), "logger.debug": None})
case("_moa_aggregator_cache_policy", agent={}, eff_model="",
     io={"load_config": {}, "resolve_moa_preset": {"aggregator": {"provider": "", "model": "m"}}})

# ── _route_may_be_custom ──
case("_route_may_be_custom", agent={"_custom_providers": [{"name": "x", "provider_key": "openai", "base_url": "https://x"}]},
     eff_provider="custom:foo", provider_lower="custom:foo", eff_base_url="https://x",
     io={"normalize_route_base_url": "https://x", "custom_provider_aliases": ["custom:foo"]})
case("_route_may_be_custom", agent={"_custom_providers": [{"name": "x", "provider_key": "y", "base_url": "https://x"}]},
     eff_provider="z", provider_lower="z", eff_base_url="https://x",
     io={"normalize_route_base_url": seq("https://x", "https://x"), "custom_provider_aliases": []})
case("_route_may_be_custom", agent={"_custom_providers": []}, eff_provider="openai", provider_lower="openai", eff_base_url="")
case("_route_may_be_custom", agent={}, eff_provider="openai", provider_lower="openai", eff_base_url="",
     io={"get_provider": None})
case("_route_may_be_custom", agent={}, eff_provider="p", provider_lower="p", eff_base_url="https://e",
     io={"get_provider": {"base_url": "https://a"}, "base_url_hostname": seq("a", "e")})
case("_route_may_be_custom", agent={}, eff_provider="custom:p", provider_lower="custom:p", eff_base_url="https://e",
     io={"get_provider": raise_("RuntimeError", "lookup"), "logger.debug": None})

# ── _provider_supplied_client ──
case("_provider_supplied_client", agent={"provider": "custom"}, client_kwargs={"base_url": "https://x"},
     io={"get_provider_profile": {"name": "p", "auth_type": "http"}, "profile.create_client": {"id": "c"}}, env=envx())
case("_provider_supplied_client", agent={"provider": ""}, client_kwargs={"base_url": "acp://x"},
     io={"get_provider_profile": raise_("ValueError", "no profile"), "_profile_for_base_url": {"name": "acp"},
         "profile.create_client": {"id": "acp-c"}}, env=envx())
case("_provider_supplied_client", agent={"provider": "none"}, client_kwargs={},
     io={"get_provider_profile": None}, env=envx())
case("_provider_supplied_client", agent={"provider": "ext"}, client_kwargs={},
     io={"get_provider_profile": {"name": "e", "auth_type": "external_process"},
         "profile.create_client": raise_("RuntimeError", "boom")}, env=envx())
case("_provider_supplied_client", agent={"provider": "http"}, client_kwargs={},
     io={"get_provider_profile": {"name": "h", "auth_type": "http"},
         "profile.create_client": raise_("RuntimeError", "boom"), "ra.logger.warning": None}, env=envx())
case("_provider_supplied_client", agent={"provider": "x"}, client_kwargs={}, env=envx(providers_import_ok=False))

# ── _ensure_route_client_headers ──
case("_ensure_route_client_headers", client_kwargs={"base_url": "https://x", "api_key": "k"}, provider="p",
     io={"route_answer": {}})
case("_ensure_route_client_headers", client_kwargs={"base_url": "https://x", "api_key": "k"}, provider="p",
     io={"route_answer": {"X-New": "v"}})
case("_ensure_route_client_headers", client_kwargs={"base_url": "https://x", "api_key": "k",
     "default_headers": {"x-new": "old"}}, provider="p", io={"route_answer": {"X-New": "v"}})
case("_ensure_route_client_headers", client_kwargs={"base_url": "b", "api_key": "a"}, provider="q",
     io={"route_answer": {"A": "1", "B": "2"}})
case("_ensure_route_client_headers", client_kwargs={"base_url": "b", "api_key": "a"}, provider="q",
     io={"route_answer": raise_("ValueError", "bad")})

# ── _gemini_native_client ──
case("_gemini_native_client", agent={}, client_kwargs={"base_url": "https://other", "api_key": "k"},
     httpx_verify=True, reason="r", shared=False, io={"is_native_gemini_base_url": False})
case("_gemini_native_client", agent={}, client_kwargs={"base_url": "https://gemini", "api_key": "k",
     "http_client": {"id": "hc"}}, httpx_verify=True, reason="r", shared=True,
     io={"is_native_gemini_base_url": True, "GeminiNativeClient": {"id": "cli"},
         "agent._client_log_context": "ctx", "ra.logger.info": None})
case("_gemini_native_client", agent={}, client_kwargs={"base_url": "https://gemini", "api_key": "k"},
     httpx_verify=False, reason="r", shared=False,
     io={"is_native_gemini_base_url": True, "agent._build_keepalive_http_client": {"id": "hc"},
         "GeminiNativeClient": {"id": "cli"}, "agent._client_log_context": "ctx", "ra.logger.info": None})
case("_gemini_native_client", agent={}, client_kwargs={"base_url": "https://gemini"},
     httpx_verify=True, reason="r2", shared=True,
     io={"is_native_gemini_base_url": True, "agent._build_keepalive_http_client": None,
         "GeminiNativeClient": {"id": "cli2"}, "agent._client_log_context": "", "ra.logger.info": None})
case("_gemini_native_client", agent={}, client_kwargs={"base_url": "", "timeout": 5},
     httpx_verify=True, reason="r3", shared=False, io={"is_native_gemini_base_url": False})

# ── _apply_switched_provider_request_overrides ──
case("_apply_switched_provider_request_overrides", agent={"_custom_providers": [{"name": "x"}], "model": "m",
     "base_url": "https://x", "request_overrides": {"service_tier": "t", "extra_body": {"old": 1}}},
     new_provider="p", io={"_custom_provider_extra_body_for_agent": {"new": 2}})
case("_apply_switched_provider_request_overrides", agent={"model": "m", "base_url": "https://x"},
     new_provider="p", io={"load_config": {}, "get_compatible_custom_providers": [],
                            "_custom_provider_extra_body_for_agent": {"k": "v"}})
case("_apply_switched_provider_request_overrides", agent={"_custom_providers": [], "model": "m", "base_url": ""},
     new_provider="p", io={"_custom_provider_extra_body_for_agent": None})
case("_apply_switched_provider_request_overrides", agent={"_custom_providers": [], "model": "m", "base_url": ""},
     new_provider="p", io={"load_config": raise_("RuntimeError", "cfg"),
                            "get_compatible_custom_providers": [],
                            "_custom_provider_extra_body_for_agent": {"k": "v"}})
case("_apply_switched_provider_request_overrides", agent={"_custom_providers": [{"name": "x"}], "model": "m", "base_url": "b"},
     new_provider="q", io={"_custom_provider_extra_body_for_agent": {"a": 1, "b": 2}})

# ── _resolve_switch_destination ──
case("_resolve_switch_destination", agent={"base_url": "https://x"}, new_model="m", new_provider="p",
     base_url="https://b", api_mode="chat_completions", capabilities={"a": 1}, old_norm="p", new_norm="p", io={})
case("_resolve_switch_destination", agent={}, new_model="m", new_provider="p", base_url="", api_mode="",
     capabilities=None, old_norm="p", new_norm="openai", io={"determine_api_mode": "chat_completions"})
case("_resolve_switch_destination", agent={"base_url": "https://old"}, new_model="m", new_provider="p",
     base_url="", api_mode="", capabilities=None, old_norm="p", new_norm="p",
     io={"determine_api_mode": "chat_completions", "resolve_native_compaction_capabilities": {"c": 1}})
case("_resolve_switch_destination", agent={}, new_model="m", new_provider="p", base_url="https://actual",
     api_mode="chat_completions", capabilities=None, old_norm="a", new_norm="b",
     io={"is_actual_route": True, "normalize_actual_base_url": "https://actual/v1",
         "resolve_native_compaction_capabilities": {}})
case("_resolve_switch_destination", agent={}, new_model="m", new_provider="opencode", base_url="https://x/v1",
     api_mode="anthropic_messages", capabilities={"k": 1}, old_norm="a", new_norm="b",
     io={"opencode_provider_family": {"family": "opencode"}})

# ── _build_switched_client ──
case("_build_switched_client", agent={"model": "m"}, new_provider="moa", api_key="k", base_url="", api_mode="", new_norm="moa",
     io={"bind_moa_runtime": None}, env=envx())
case("_build_switched_client", agent={"model": "m", "base_url": "https://old"}, new_provider="bedrock", api_key="k",
     base_url="", api_mode="anthropic_messages", new_norm="bedrock", io={"bind_bedrock_runtime": None}, env=envx())
case("_build_switched_client", agent={"provider": "anthropic", "model": "m", "api_key": "", "base_url": "b"},
     new_provider="anthropic", api_key="", base_url="https://a", api_mode="anthropic_messages", new_norm="anthropic",
     io={"native_anthropic_fallback_token": "fallback", "get_provider_request_timeout": 30,
         "build_anthropic_client": {"id": "cli"}, "anthropic_route_is_oauth": False}, env=envx())
case("_build_switched_client", agent={"provider": "minimax-oauth", "model": "m", "api_key": "tok"},
     new_provider="minimax-oauth", api_key="tok", base_url="https://m", api_mode="anthropic_messages", new_norm="minimax",
     io={"build_minimax_oauth_token_provider": {"tok": "fn"}, "get_provider_request_timeout": None,
         "build_anthropic_client": {"id": "cli"}, "anthropic_route_is_oauth": True}, env=envx())
case("_build_switched_client", agent={"provider": "openai", "model": "m", "api_key": "old", "base_url": "https://old"},
     new_provider="openai", api_key="new", base_url="https://new", api_mode="chat_completions", new_norm="openai",
     io={"load_config_readonly": {}, "get_compatible_custom_providers": [],
         "apply_custom_provider_tls_to_client_kwargs": None, "get_provider_request_timeout": 30,
         "agent._apply_client_headers_for_base_url": None, "agent._create_openai_client": {"id": "oai"}}, env=envx())

# ── _swap_switch_runtime ──
SWAP_IO = {"agent._read_reasoning_echo_from_config": {"flag": True}, "reset_route_verdicts": None,
           "bind_moa_runtime": None, "sync_credential_pool_entry_id": None}
case("_swap_switch_runtime", agent={"model": "old", "base_url": "https://x"}, new_model="m", new_provider="p",
     api_key="k", base_url="https://x", api_mode="chat_completions", old_provider="p", old_norm="p", new_norm="p",
     io=dict(SWAP_IO), env=envx())
case("_swap_switch_runtime", agent={}, new_model="m", new_provider="q", api_key="k", base_url="",
     api_mode="chat_completions", old_provider="p", old_norm="p", new_norm="q", io=dict(SWAP_IO), env=envx())
case("_swap_switch_runtime", agent={}, new_model="m", new_provider="q", api_key="k", base_url="https://q",
     api_mode="chat_completions", old_provider="p", old_norm="p", new_norm="q",
     io={**SWAP_IO, "load_route_pool": {"id": "pool"}}, env=envx())
case("_swap_switch_runtime", agent={}, new_model="m", new_provider="p", api_key=None, base_url="https://x",
     api_mode="anthropic_messages", old_provider="p", old_norm="p", new_norm="p",
     io={**SWAP_IO, "get_provider_request_timeout": 30, "build_anthropic_client": {"id": "cli"},
         "anthropic_route_is_oauth": False}, env=envx())
case("_swap_switch_runtime", agent={}, new_model="m", new_provider="q", api_key="k", base_url="https://q",
     api_mode="chat_completions", old_provider="p", old_norm="p", new_norm="q",
     io={**SWAP_IO, "load_route_pool": raise_("RuntimeError", "pool"), "logger.warning": None}, env=envx())

# ── _resolve_switch_context_length ──
case("_resolve_switch_context_length", agent={}, snapshot={}, env=envx(has_lmstudio=False))
case("_resolve_switch_context_length", agent={}, snapshot={},
     io={"load_config": {}, "get_compatible_custom_providers": [], "config_context_length_for_runtime": 4096,
         "set_config_context_length": None}, env=envx(has_lmstudio=False))
case("_resolve_switch_context_length", agent={}, snapshot={},
     io={"load_config": {}, "get_compatible_custom_providers": [], "config_context_length_for_runtime": None,
         "get_custom_provider_context_length": 8192, "set_config_context_length": None},
     env=envx(has_lmstudio=True))
case("_resolve_switch_context_length", agent={}, snapshot={},
     io={"load_config": {}, "get_compatible_custom_providers": [], "config_context_length_for_runtime": 4096,
         "set_config_context_length": None, "agent._ensure_lmstudio_runtime_loaded": raise_("RuntimeError", "lm")},
     env=envx(has_lmstudio=True))
case("_resolve_switch_context_length", agent={}, snapshot={},
     io={"load_config": raise_("RuntimeError", "cfg"), "get_compatible_custom_providers": [],
         "config_context_length_for_runtime": None, "get_custom_provider_context_length": None,
         "set_config_context_length": None, "agent._ensure_lmstudio_runtime_loaded": 4096,
         "agent._lmstudio_load_was_unverified": True, "logger.warning": None,
         "agent._effective_lmstudio_context_length": 4096}, env=envx(has_lmstudio=True, has_lmstudio_unverified=True, has_effective_lmstudio=True))

# ── _update_switch_compressor ──
case("_update_switch_compressor", agent={"model": "m", "base_url": "b", "api_key": "k", "provider": "p", "api_mode": "chat_completions"},
     custom_providers=[], effective_context_length=4096, snapshot={},
     io={"get_model_context_length": 8192, "agent.context_compressor.update_model": None,
         "revalidate_compression_feasibility": None})
case("_update_switch_compressor", agent={"model": "m", "base_url": "b", "api_key": None, "provider": "p", "api_mode": ""},
     custom_providers=None, effective_context_length=None, snapshot={},
     io={"load_config": {}, "get_compatible_custom_providers": [], "get_model_context_length": 4096,
         "agent.context_compressor.update_model": None, "revalidate_compression_feasibility": None})
case("_update_switch_compressor", agent={"model": "m", "base_url": "b", "api_key": 5, "provider": "p"},
     custom_providers=[], effective_context_length=4096, snapshot={},
     io={"get_model_context_length": raise_("ValueError", "len"), "logger.debug": None})
case("_update_switch_compressor", agent={"model": "m", "base_url": "b", "api_key": "k", "provider": "p"},
     custom_providers=[{"name": "x"}], effective_context_length=2048, snapshot={},
     io={"get_model_context_length": 4096, "agent.context_compressor.update_model": None,
         "revalidate_compression_feasibility": None})
case("_update_switch_compressor", agent={"model": "m", "base_url": "b", "api_key": "k", "provider": "p"},
     custom_providers=[], effective_context_length=100, snapshot={},
     io={"get_model_context_length": 4096, "agent.context_compressor.update_model": None,
         "revalidate_compression_feasibility": None})

# ── _finish_switch ──
case("_finish_switch", agent={}, new_provider="p", old_norm="a", new_norm="b", env=envx())
case("_finish_switch", agent={"_fallback_chain": [{"provider": "a"}, {"provider": "b"}, {"provider": "c"}]},
     new_provider="c", old_norm="a", new_norm="c",
     io={"load_config": {}, "get_compatible_custom_providers": [],
         "_custom_provider_extra_body_for_agent": None}, env=envx())
case("_finish_switch", agent={"_fallback_chain": [{"provider": "c"}]},
     new_provider="c", old_norm="a", new_norm="a",
     io={"_custom_provider_extra_body_for_agent": {"x": 1}}, env=envx())
case("_finish_switch", agent={"_fallback_chain": [{"provider": "d"}], "request_overrides": {"keep": 1}},
     new_provider="p", old_norm="a", new_norm="b",
     io={"_custom_provider_extra_body_for_agent": {"extra": 2}}, env=envx())
case("_finish_switch", agent={"_fallback_chain": [{"provider": "a"}]},
     new_provider="p", old_norm="a", new_norm="b",
     io={"load_config": {}, "get_compatible_custom_providers": [],
         "_custom_provider_extra_body_for_agent": raise_("RuntimeError", "ovr"), "logger.debug": None}, env=envx())

# ── _persist_switch_billing_route ──
case("_persist_switch_billing_route", agent={})
case("_persist_switch_billing_route", agent={"session_id": "s"})
case("_persist_switch_billing_route", agent={"_session_db": {}, "session_id": "s", "provider": "openai",
     "base_url": "https://x", "api_mode": "chat_completions"}, io={"agent_interface": None})
case("_persist_switch_billing_route", agent={"_session_db": {}, "session_id": "s", "provider": "p",
     "base_url": "b", "api_mode": ""}, io={"agent_interface": {"store": True}, "session_db.update_session_billing_route": None})
case("_persist_switch_billing_route", agent={"_session_db": {}, "session_id": "s", "provider": "p"},
     io={"agent_interface": {"store": True}, "store.usage.set_billing_route": raise_("RuntimeError", "db"),
         "logger.warning": None})

# ── _log_empty_non_final_heal ──
case("_log_empty_non_final_heal", healed=1,
     io={"_session_id_for_heal_log": "s1", "_heal_escalation_threshold": 3, "publish_lru": None,
         "ra.logger.warning": None}, env=envx())
case("_log_empty_non_final_heal", healed=2,
     io={"_session_id_for_heal_log": None, "_heal_escalation_threshold": 1, "publish_lru": None,
         "ra.logger.error": None, "heal_user_notice.add": None, "heal_pending_notice.set": None}, env=envx())
case("_log_empty_non_final_heal", healed=1,
     io={"_session_id_for_heal_log": "s2", "_heal_escalation_threshold": 0, "publish_lru": None,
         "ra.logger.warning": None}, env=envx())
case("_log_empty_non_final_heal", healed=3,
     io={"_session_id_for_heal_log": "s3", "_heal_escalation_threshold": 2, "publish_lru": None,
         "ra.logger.error": None, "heal_user_notice.add": None, "heal_pending_notice.set": None},
     env=envx(heal_state={"s3": {"count": 1, "window_start": 990, "escalated": False,
                                "total_events": 1, "total_healed": 2}}))
case("_log_empty_non_final_heal", healed=1,
     io={"_session_id_for_heal_log": "s4", "_heal_escalation_threshold": 3, "publish_lru": None,
         "ra.logger.warning": None},
     env=envx(heal_state={"s4": {"count": 1, "window_start": 999, "escalated": True,
                                "total_events": 5, "total_healed": 9}}))

# ── looks_like_degenerate_final ──
case("looks_like_degenerate_final", text="ok", user_message=None, env=envx())
case("looks_like_degenerate_final", text="", user_message=None, env=envx())
case("looks_like_degenerate_final", text="Done.", user_message=None, env=envx())
case("looks_like_degenerate_final", text="是", user_message="你好", env=envx(),
     io={"_summarize_user_message_for_log": "你好"})
case("looks_like_degenerate_final", text="是", user_message="hello", env=envx(),
     io={"_summarize_user_message_for_log": "hello"})
case("looks_like_degenerate_final", text="!A", user_message=None, env=envx())

# ── promoted_reasoning_announces_action ──
case("promoted_reasoning_announces_action", text="", io={})
case("promoted_reasoning_announces_action", text="  ok  ", io={"promoted_reasoning_plan_tail_re.search": False})
case("promoted_reasoning_announces_action", text="I will do it", io={"promoted_reasoning_plan_tail_re.search": True})
case("promoted_reasoning_announces_action", text="I'll try that next", io={"promoted_reasoning_plan_tail_re.search": True})
case("promoted_reasoning_announces_action", text="noise", io={"promoted_reasoning_plan_tail_re.search": False})

# ── socket leaves ──
case("_socket_from_stream", stream={"_sock": {"id": "s1"}})
case("_socket_from_stream", stream={"get_extra_info_callable": True},
     io={"stream.get_extra_info": {"id": "s2"}})
case("_socket_from_stream", stream={"stream": {"_sock": {"id": "s3"}}})
case("_socket_from_stream", stream={"_stream": {"extra_callable": True}},
     io={"stream._stream.extra": {"id": "s4"}})
case("_socket_from_stream", stream={})

case("_socket_from_candidate", candidate={"_network_stream": {"_sock": {"id": "s1"}}})
case("_socket_from_candidate", candidate={"_stream": {"get_extra_info_callable": True}},
     io={"stream.get_extra_info": {"id": "s2"}})
case("_socket_from_candidate", candidate={"_sock": {"id": "s3"}})
case("_socket_from_candidate", candidate={})
case("_socket_from_candidate", candidate={"_network_stream": {}})

case("_socket_from_response", response={"extensions": {"network_stream": {"id": "start"}}},
     io={"connection_candidates": [{"_sock": {"id": "s1"}}]})
case("_socket_from_response", response={"stream": {"id": "st"}},
     io={"connection_candidates": [{"_sock": {"id": "s2"}}]})
case("_socket_from_response", response={})
case("_socket_from_response", response={"extensions": {}}, io={"connection_candidates": []})
case("_socket_from_response", response={"extensions": {"network_stream": {}}},
     io={"connection_candidates": [{}, {"_sock": {"id": "s3"}}]})

case("_shutdown_socket", sock={"settimeout_callable": True, "shutdown_callable": True},
     io={"sock.settimeout": None, "sock.shutdown": None})
case("_shutdown_socket", sock={"settimeout_callable": False, "shutdown_callable": True},
     io={"sock.shutdown": None})
case("_shutdown_socket", sock={"shutdown_callable": True},
     io={"sock.shutdown": raise_("OSError", "down")})
case("_shutdown_socket", sock={"settimeout_callable": True, "shutdown_callable": True},
     io={"sock.settimeout": raise_("OSError", "timeout"), "sock.shutdown": None})
case("_shutdown_socket", sock={"shutdown_callable": True}, io={"sock.shutdown": None})


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