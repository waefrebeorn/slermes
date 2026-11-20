#!/usr/bin/env python3
"""Generate tests/fixtures/acp_gateway_server_flow2.jsonl (the 31-func faithful subset).

Each case drives the LIVE def via oracle_acp_gateway_server_flow2.drive under a
scripted world; the ordered seam trace's returns become the fixture's per-name
seam queues. Replayed in the C harness and the oracle reproduces identical seam
behaviour, so the differential compares the port's own control flow. Every func
gets >= 6 cases. Raise scripts use the full exception hierarchy; BaseException-
only kinds (SystemExit / CancelledError) exercise real passthrough.
"""
import importlib.util
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location(
    "oracle_acp_gateway_server_flow2", HERE / "oracle_acp_gateway_server_flow2.py")
oracle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(oracle)
oracle.load_ns()

CASES = []
R = lambda msg="boom": {"__raise__": "RuntimeError", "message": msg}
SX = lambda msg="die": {"__raise__": "SystemExit", "message": msg}
CE = lambda msg="cancelled": {"__raise__": "CancelledError", "message": msg}
VE = lambda msg="bad": {"__raise__": "ValueError", "message": msg}
GCE = lambda msg="x": {"__raise__": "GatewayClientError", "message": msg}
GRPC = lambda msg="not_found": {"__raise__": "GatewayRPCError", "message": msg}


_SELF_EXACT = set(oracle.SELF_METHODS) | set(oracle.FUNCS)
_ALIAS = {
    "client": "client", "rpc": "rpc", "conn_session_update": "conn.session_update",
    "connection___aexit__": "connection.__aexit__", "asyncio_gather": "asyncio.gather",
    "teardown_cancel": "teardown_cancel", "bypass_route": "_bypass_route",
    "slash_command": "_slash_command", "tools_changed_notice": "_tools_changed_notice",
}


def _canon_key(k):
    if k in _ALIAS:
        return _ALIAS[k]
    cand = "_" + k.lstrip("_")
    return cand if cand in _SELF_EXACT else k


def add(func, argv=None, kw=None, **world):
    CASES.append((func, list(argv or []), dict(kw or {}),
                  {_canon_key(k): v for k, v in world.items()}))


def st(**kw):
    return kw


def js(x):
    if isinstance(x, set):
        return sorted(js(v) for v in x)
    if isinstance(x, (list, tuple)):
        return [js(v) for v in x]
    if isinstance(x, dict):
        out = {}
        for k, v in x.items():
            key = "|".join(str(p) for p in k) if isinstance(k, (tuple, list)) else k
            out[str(key)] = js(v)
        return out
    return x


def emit():
    out = []
    for func, argv, kw, world in CASES:
        r = oracle.drive(func, argv, kw, world)
        seams = {}
        for ev in r["trace"]:
            seams.setdefault(ev["ev"], []).append(ev["ret"])
        out.append({"func": func, "in": {"argv": js(argv), "kw": js(kw), "seams": seams}})
    counts = {}
    for c in out:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    if set(counts) != set(oracle.FUNCS):
        missing = set(oracle.FUNCS) - set(counts)
        raise SystemExit("funcs with no cases: %r" % missing)
    bad = {k: v for k, v in counts.items() if v < 6}
    if bad:
        raise SystemExit("too few cases: %r" % bad)
    fx = HERE / "fixtures" / "acp_gateway_server_flow2.jsonl"
    fx.parent.mkdir(exist_ok=True)
    with open(fx, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(json.dumps(c, ensure_ascii=False, separators=(",", ":")) for c in out) + "\n")
    print("wrote %d cases across %d funcs -> %s" % (len(out), len(counts), fx))


# 1 __init__
for _ in range(6):
    add("__init__", [st()], get_hermes_home=["/h"], gateway_profile_home=["/h"],
        uuid=["u1"])

# 2 on_connect
add("on_connect", [st(_conn=None), {"_conn": {"add_observer": True}}], add_observer=[None])
add("on_connect", [st(_conn=None), {"_conn": {"add_observer": False}}])
add("on_connect", [st(_conn=None), {}])
add("on_connect", [st(_conn=None), {"_conn": None}])
add("on_connect", [st(_conn=None), {"_conn": {"add_observer": True}}], add_observer=[SX()])
add("on_connect", [st(_conn=True), {"_conn": {"add_observer": True}}], add_observer=[None])

# 3 _observe_client_message
EV = lambda method, direction="incoming", params=None: {
    "message": {"method": method, "params": params or {}}, "direction": direction}
add("_observe_client_message", [st(_form_elicitation=False), EV("initialize")],
    client_supports_form_elicitation=[True])
add("_observe_client_message", [st(_form_elicitation=True), EV("initialize", params={"x": 1})],
    client_supports_form_elicitation=[False])
add("_observe_client_message", [st(), EV("initialize", direction="outgoing")])
add("_observe_client_message", [st(), EV("prompt")])
add("_observe_client_message", [st(), {"direction": "incoming", "message": None}])
add("_observe_client_message", [st(_form_elicitation=False), EV("initialize")],
    client_supports_form_elicitation=[VE()])

# 4 initialize
add("initialize", [st()], build_auth_methods=[[]])
add("initialize", [st()], build_auth_methods=[["m"]])
add("initialize", [st()], build_auth_methods=[None])
add("initialize", [st()], build_auth_methods=[SX()])
add("initialize", [st()], build_auth_methods=[CE()])
add("initialize", [st()], build_auth_methods=[VE()])

# 5 authenticate
for m in ["m1", "m2", "", None]:
    add("authenticate", [st(), m], authenticate=[{"ok": True}])
add("authenticate", [st(), "m1"], authenticate=[SX()])
add("authenticate", [st(), "m1"], authenticate=[GCE()])

# 6 new_session
add("new_session", [st(), "/a"], client=[None], rpc=[{"session_create": {"sources": ["acp"]},
    "capabilities": ["acp-editor-policy-v1"], "session_id": "s1", "info": {"title": "T"}}],
    env_launch_flags=[{}], adopt=[None], request_live_reasoning=[None], session_fields=[{}])
add("new_session", [st(), "/a", [{"m": 1}]], client=[None],
    rpc=[{"session_create": {"sources": ["acp"]}, "capabilities": ["acp-editor-policy-v1"],
          "session_id": "s1", "info": {"title": "T"}}],
    env_launch_flags=[{"safe_mode": True}], bypass_route=[{"model": "m", "provider": "p"}],
    adopt=[None], request_live_reasoning=[None], session_fields=[{}])
add("new_session", [st(), "/a"], client=[None], rpc=[{"session_create": {}, "capabilities": []}])
add("new_session", [st(), "/a"], client=[VE()])
add("new_session", [st(), "/a"], client=[None], rpc=[VE()])
add("new_session", [st(), "/a"], client=[None],
    rpc=[{"session_create": {"sources": ["acp"]}, "capabilities": ["acp-editor-policy-v1"], "session_id": "s1"}],
    env_launch_flags=[{}], adopt=[None], request_live_reasoning=[None], session_fields=[SX()])

# 7 _client
add("_client", [st(_failure=None, _gateway=True, _transport_closed=False)])
add("_client", [st(_failure="stale", _gateway=True)], teardown=[None])
add("_client", [st(_failure=None, _gateway=None, _home="/h", _transport_closed=False)],
    rpc=[{"profile_id": "/h"}], reattach=[None], create_task=[None])
add("_client", [st(_failure=None, _gateway=None, _home="/h", _transport_closed=False)],
    rpc=[{"profile_id": "other"}], connection___aexit__=[None])
add("_client", [st(_failure=None, _gateway=True, _transport_closed=True)],
    teardown=[None], transport_closed=[True])
add("_client", [st(_failure=None, _gateway=True)], rpc=[SX()], transport_closed=[SX()])

# 8 _adopt
add("_adopt", [st(), "s1", {"info": {"x": 1}}], project_mode=[None])
add("_adopt", [st(), "s1", {}, 2], project_mode=[None])
add("_adopt", [st(), "s1", {"info": None}, None, "GW"], project_mode=[None])
add("_adopt", [st(), "s1", {}, None, "GW"], project_mode=[VE()])
add("_adopt", [st(), "s1", {}], project_mode=[SX()])
add("_adopt", [st(), "s1", {"info": {"title": "t"}}], project_mode=[None])

# 9 _move_workspace
add("_move_workspace", [st(), "GW", "s1", "/a"], mutations_apply=[None],
    mutations_acknowledge=[None], rpc=[{"cwd": "/a"}])
add("_move_workspace", [st(_unacked={"s1": 1}), "GW", "s1", "/a"],
    mutations_apply=[None], mutations_acknowledge=[None], rpc=[{}])
add("_move_workspace", [st(), "GW", "s1", "/a"], mutations_apply=[VE()])
add("_move_workspace", [st(), "GW", "s1", "/a"], mutations_apply=[None], mutations_acknowledge=[SX()])
add("_move_workspace", [st(), "GW", "s1", "/a"], mutations_apply=[None], mutations_acknowledge=[None], rpc=[GCE()])
add("_move_workspace", [st(), "GW", "s1", "/a"], mutations_apply=[None], mutations_acknowledge=[None], rpc=[None])

# 10 _rebind_editor_mcp
add("_rebind_editor_mcp", [st(), "GW", "s1", {"m": 1}],
    mutations_apply=[None], mutations_acknowledge=[None])
add("_rebind_editor_mcp", [st(_unacked={"s1": 1}), "GW", "s1", {"m": 1}],
    mutations_apply=[None], mutations_acknowledge=[None])
add("_rebind_editor_mcp", [st(), "GW", "s1", {}], mutations_apply=[VE()])
add("_rebind_editor_mcp", [st(), "GW", "s1", {}], mutations_apply=[None], mutations_acknowledge=[SX()])
add("_rebind_editor_mcp", [st(), "GW", "s1", {}], mutations_apply=[None], mutations_acknowledge=[None])
add("_rebind_editor_mcp", [st(), "GW", "s1", None], mutations_apply=[None], mutations_acknowledge=[None])

# 11 load_session / 12 resume_session -> _resume
for fn in ("load_session", "resume_session"):
    add(fn, [st(), "/a", "s1"], resume=[{"session_id": "s1"}])
    add(fn, [st(), "/a", "s1", [{"x": 1}]], resume=[{"session_id": "s1"}])
    add(fn, [st(), "/a", "s1"], resume=[SX()])
    add(fn, [st(), "/a", "s1"], resume=[VE()])
    add(fn, [st(), "/a", "s1", []], resume=[{}])
    add(fn, [st(), "/a", "s1"], resume=[{"session_id": "s2"}])

# 14 cancel
add("cancel", [st(_snapshots={"s1": {}}, _admissions={"s1": "a1"}), "s1"], cancel_admission=[None])
add("cancel", [st(_snapshots={"s1": {}}, _aliases={"h": "s1"}, _admissions={"s1": "a1"}), "h"], cancel_admission=[None])
add("cancel", [st(_snapshots={"s1": {}}), "s1"])
add("cancel", [st(_snapshots={"s1": {}}, _submitting={"s1"}), "s1"])
add("cancel", [st(_snapshots={"s1": {}}, _admissions={"s1": "a1"}), "s1"], cancel_admission=[VE()])
add("cancel", [st(_snapshots={}), "missing"])

# 15 fork_session
add("fork_session", [st(), "/a", "s1"], info=[{"cwd": "/a"}],
    mutations_apply=[{"branched_session_id": "c1"}], rpc=[{"prompts": []}, {}],
    adopt=[None], request_live_reasoning=[None], session_fields=[{"models": []}])
add("fork_session", [st(), "/a", "s1", [{"m": 1}]],
    info=[{"cwd": "/a"}], mutations_apply=[{"branched_session_id": "c1"}],
    rpc=[{}, {}], adopt=[None], request_live_reasoning=[None], session_fields=[{}])
add("fork_session", [st(), "/a", "s1"], info=[VE()])
add("fork_session", [st(), "/a", "s1"], info=[{}], mutations_apply=[VE()])
add("fork_session", [st(), "/a", "s1"], info=[{}], mutations_apply=[{"branched_session_id": "c1"}], rpc=[SX()])
add("fork_session", [st(), "/a", "s1"], info=[{}], mutations_apply=[{"branched_session_id": "c1"}],
    rpc=[{}, {}], adopt=[None], request_live_reasoning=[None], session_fields=[SX()])

# 16 set_session_model
add("set_session_model", [st(), "m1", "s1"], parse_model_input=[["p", "m1"]],
    mutations_apply=[{"status": "ok"}], rpc=[{"prompts": []}], adopt=[None],
    mutations_acknowledge=[None])
add("set_session_model", [st(), "m1", "s1"], parse_model_input=[["", "m1"]],
    mutations_apply=[{"status": "ok"}], rpc=[{}], adopt=[None], mutations_acknowledge=[None])
add("set_session_model", [st(), "m1", "s1"], parse_model_input=[["p", "m1"]],
    mutations_apply=[{"status": "cancelled"}])
add("set_session_model", [st(), "m1", "s1"], parse_model_input=[["p", "m1"]],
    mutations_apply=[{"status": "deferred"}], rpc=[{}], adopt=[None], mutations_acknowledge=[None])
add("set_session_model", [st(), "m1", "s1"], parse_model_input=[VE()])
add("set_session_model", [st(), "m1", "s1"], parse_model_input=[["p", "m1"]], mutations_apply=[SX()])

# 17 _confirm_deferred_pick
ASK = {"target_model": "m2", "committed_model": "m1", "committed_provider": "p",
       "confirm": "TOKEN", "payload": {"model": "m2"}}
add("_confirm_deferred_pick", [st(), "s1", "e1", ASK], model_confirmer=[None],
    refused_pick_text=["refused"])
add("_confirm_deferred_pick", [st(), "s1", "e1", ASK], model_confirmer=[False],
    refused_pick_text=["refused"])
add("_confirm_deferred_pick", [st(), "s1", "e1", ASK], model_confirmer=[True],
    rpc=[{"revision": 1, "execution_generation": 2}, {"status": "ok"}],
    adopt=[None], deferred_pick_text=["deferred"])
add("_confirm_deferred_pick", [st(_snapshots={"s1": {}}), "s1", "e1", ASK],
    model_confirmer=[True], rpc=[{"revision": 1, "execution_generation": 2}, {"status": "confirmed"}],
    adopt=[None], _model_text=["switched"])
add("_confirm_deferred_pick", [st(), "s1", "e1", ASK], model_confirmer=[GCE("bad")],
    refused_pick_text=["refused"], logger_info=[None])
add("_confirm_deferred_pick", [st(), "s1", "e1", ASK], model_confirmer=[True], rpc=[SX()])

# 18 _model_confirmer
add("_model_confirmer", [st(_conn=None), "s1"])
add("_model_confirmer", [st(_conn=True), "s1"])
for x in [None, True, False]:
    add("_model_confirmer", [st(_conn=True), "s1"], rpc=[x], confirmation_title=["t"])
add("_model_confirmer", [st(_conn=True), "s1"], rpc=[VE()])

# 19 set_session_mode
for m in ["default", "ask", "acceptEdits", "bypassPermissions", "bogus"]:
    add("set_session_mode", [st(), m, "s1"],
        held_session=["s1"], set_edit_policy=[None])
add("set_session_mode", [st(), "ask", "h"], held_session=[VE()])

# 20 set_config_option
add("set_config_option", [st(_modes={"s1": "ask"}), "other", "s1", "v"],
    held_session=["s1"], rpc=[{"x": 1}], model_state=[[]], session_config_options=[[]])
add("set_config_option", [st(), "model", "s1", "m1"], held_session=["s1"],
    set_session_model=[None], rpc=[{}], model_state=[[]], session_config_options=[[]])
add("set_config_option", [st(), "reasoning_effort", "s1", "high"], held_session=["s1"],
    reasoning_choices=[["high"]], idle_control=[None], rpc=[{}], model_state=[[]], session_config_options=[[]])
add("set_config_option", [st(), "service_tier", "s1", "fast"], held_session=["s1"],
    rpc=[{}], model_state=[[]], session_config_options=[[]])
add("set_config_option", [st(), "edit_approval_policy", "s1", "bogus"], held_session=["s1"])
add("set_config_option", [st(), "model", "s1", 42], held_session=["s1"])

# 22 _idle_control
add("_idle_control", [st(), "s1", "edit_policy", {"policy": "ask"}, "Mode"],
    client=[None], mutations_apply=[None], mutations_acknowledge=[None])
add("_idle_control", [st(), "s1", "reasoning", {"effort": None}, "Reasoning effort"],
    client=[None], mutations_apply=[GCE("session_busy")], conn_session_update=[None])
add("_idle_control", [st(), "s1", "reasoning", {"effort": "high"}, "Reasoning effort"],
    client=[None], mutations_apply=[GCE("unknown_execution")],
    unknown_recovery=["unknown_execution: lost"], conn_session_update=[None])
add("_idle_control", [st(), "s1", "x", {}, "X"], client=[None], mutations_apply=[VE()])
add("_idle_control", [st(), "s1", "x", {}, "X"], client=[SX()])
add("_idle_control", [st(_conn=None), "s1", "x", {}, "X"],
    client=[None], mutations_apply=[GCE("session_busy")])

# 23 list_sessions
add("list_sessions", [st(_home="/h")], catalog_sessions=[[{"session_id": "a", "cwd": "/a"}]])
add("list_sessions", [st(_home="/h"), None, "/a"],
    catalog_sessions=[[{"session_id": "a", "cwd": "/a"}, {"session_id": "b", "cwd": "/b"}]])
add("list_sessions", [st(_home="/h"), "b"],
    catalog_sessions=[[{"session_id": "a", "cwd": "/a"}, {"session_id": "b", "cwd": "/b"}]])
add("list_sessions", [st(_home="/h"), "zz"], catalog_sessions=[[{"session_id": "a", "cwd": "/a"}]])
add("list_sessions", [st(_home="/h")], catalog_sessions=[[{"title": "t"}]])
add("list_sessions", [st(_home="/h")], catalog_sessions=[VE()])

# 24 prompt
BASE = st(_aliases={}, _snapshots={"s1": {}}, _conn=True, _usage={})
add("prompt", [BASE, {"text": "hi"}, "s1"], has_unknown=[False], prepare_submit=[None])
add("prompt", [BASE, {"text": "hi"}, "h"], has_unknown=[False], prepare_submit=[("k", "i", ("hi", []))],
    run_slash=[False, "hi"], client=[None], rpc=[{"admission_id": "a1", "status": "started"}],
    admission_from_receipt=["a1"], after_ack=[None], changed_wait_for=[None],
    finished=[{"outcome": "end_turn"}])
add("prompt", [st(_snapshots={}), {"text": "hi"}, "s1"])
add("prompt", [BASE, {"text": "hi"}, "s1"], has_unknown=[True], refresh_unknown_for_prompt=[True])
add("prompt", [BASE, {"text": "hi"}, "s1"], has_unknown=[False], prepare_submit=[VE()])
add("prompt", [BASE, {"text": "hi"}, "s1"], has_unknown=[False], prepare_submit=[("k", "i", ("hi", []))],
    run_slash=[True, None])
add("prompt", [BASE, {"text": "hi"}, "s1"], has_unknown=[False], prepare_submit=[("k", "i", ("hi", []))],
    run_slash=[False, "hi"], client=[None], rpc=[{"voice_stopped": True}])

# 26 _prepare_submit
add("_prepare_submit", [st(_unacked={}), "s1", {"text": "hi"}],
    content_blocks=["hi"], submit_fingerprint=["k"], stage_user_content=[("hi", [])])
add("_prepare_submit", [st(_unacked={"s1": ("k", "i", ("hi", []))}), "s1", {"text": "hi"}],
    content_blocks=["hi"], submit_fingerprint=["k"])
add("_prepare_submit", [st(_unacked={"s1": ("other", "i", ("hi", []))}), "s1", {"text": "hi"}],
    content_blocks=["hi"], submit_fingerprint=["k"], stage_user_content=[("hi", [])])
add("_prepare_submit", [st(_unacked={}, _pending_cancels={"s1"}), "s1", {"text": "hi"}],
    content_blocks=["hi"], submit_fingerprint=["k"], stage_user_content=[("hi", [])])
add("_prepare_submit", [st(_unacked={}), "s1", {"text": "hi"}],
    content_blocks=["hi"], submit_fingerprint=["k"], stage_user_content=[VE()])
add("_prepare_submit", [st(_unacked={}), "s1", {"text": "hi"}], content_blocks=[VE()])

# 30 _refresh_unknown
add("_refresh_unknown", [st(_snapshots={"s1": {"pending": [{"status": "unknown"}], "replay_epoch": 1, "last_sequence": 2}}), "s1"],
    client=[None], rpc=[{"pending": [], "replay_epoch": 2, "last_sequence": 3}])
add("_refresh_unknown", [st(_snapshots={"s1": {"pending": []}}), "s1"], client=[None])
add("_refresh_unknown", [st(_snapshots={}), "s1"], client=[None])
add("_refresh_unknown", [st(_snapshots={"s1": {"pending": [{"status": "unknown"}]}}), "s1"],
    client=[None], rpc=[{"pending": [], "replay_epoch": 1, "last_sequence": 0}])
add("_refresh_unknown", [st(_snapshots={"s1": {"pending": [{"status": "unknown"}]}}), "s1"], client=[VE()])
add("_refresh_unknown", [st(_snapshots={"s1": {"pending": [{"status": "unknown"}]}}), "s1"],
    client=[None], rpc=[SX()])

# 31 _events
FR = lambda **kw: kw
add("_events", [st(_gateway=True)], events_get=[FR(params={})])
add("_events", [st(_gateway=True)], events_get=[( "s1", "a1"), FR(params={})], recover=[None])
add("_events", [st(_gateway=True, _snapshots={"s1": {"replay_epoch": 1, "last_sequence": 1}})],
    events_get=[FR(params={"session_id": "s1", "replay_epoch": 2, "seq": 3, "execution_generation": 1})], project=[None])
add("_events", [st(_gateway=True)], events_get=[VE()], logger_debug=[None])
add("_events", [st(_gateway=True)], events_get=[SX()], logger_debug=[None])
add("_events", [st(_gateway=True)], events_get=[CE()])

# 32 _project
add("_project", [st(), {"type": "session.info", "session_id": "s1", "payload": {}}],
    project_session_state=[None])
add("_project", [st(), {"type": "approval.request", "session_id": "s1", "payload": {"prompt_id": "p"}}],
    gap_guard=[False], permission=[None])
add("_project", [st(), {"type": "message.delta", "session_id": "s1", "payload": {"text": "hi"}}],
    gap_guard=[False], text=["hi"])
add("_project", [st(), {"type": "message.complete", "session_id": "s1", "payload": {"outcome": "ok"}}],
    gap_guard=[False], finish_reply=[None])
add("_project", [st(), {"type": "reasoning.delta", "session_id": "s1", "payload": {}}],
    gap_guard=[False], project_side_update=[None])
add("_project", [st(), {"type": "x", "session_id": "s1"}], gap_guard=[VE()])

# 33 _gap_guard
add("_gap_guard", [st(), "x", "s1", "a1"])
add("_gap_guard", [st(_editor_mcp={"s1": {"m": 1}}, _resumes={}, _inflight=set()), "session.replay_gap", "s1", None],
    rpc=[{"prompts": [], "pending": []}], adopt=[None], permission=[None], recover=[None])
add("_gap_guard", [st(_cut={"a1": "  old  "}, _segments={}), "tool.start", "s1", "a1"],
    end_message=[None])
add("_gap_guard", [st(_cut={"a1": ""}), "message.delta", "s1", "a1"])
add("_gap_guard", [st(_cut={"a1": "x"}), "message.interim", "s1", "a1"], end_message=[None])
add("_gap_guard", [st(_editor_mcp={}, _resumes={}), "session.replay_gap", "s1", None], rpc=[VE()])

# 37 _answer_clarify
add("_answer_clarify", [st(_form_elicitation=False, _conn=True), "s1", {"prompt_id": "p", "execution_generation": 1}],
    build_clarify_permission=[({}, [], {"skip": "SKIP"})], request_permission=[{"outcome": {"option_id": "skip"}}],
    close_card=[None], rpc=[None])
add("_answer_clarify", [st(_form_elicitation=True, _conn=True), "s1", {"prompt_id": "p", "execution_generation": 1}],
    build_clarify_elicitation=[{}], send_request=[{"answer": "a"}], elicited_answer=["a"], rpc=[None])
add("_answer_clarify", [st(_form_elicitation=False, _conn=True), "s1", {"prompt_id": "p", "execution_generation": 1}],
    build_clarify_permission=[({}, [], {})], request_permission=[VE()], logger_info=[None])
add("_answer_clarify", [st(_form_elicitation=True, _conn=True), "s1", {"prompt_id": "p"}],
    build_clarify_elicitation=[{}], send_request=[SX()], logger_info=[None])
add("_answer_clarify", [st(_form_elicitation=False, _conn=True), "s1", {"prompt_id": "p", "execution_generation": 1}],
    build_clarify_permission=[({}, [], {"x": "a"})], request_permission=[{"outcome": {}}], close_card=[None])
add("_answer_clarify", [st(_form_elicitation=False, _conn=None), "s1", {"prompt_id": "p", "execution_generation": 1}])

# 38 _session_fields
add("_session_fields", [st(_home="/h", _modes={}, _gateway=True, _conn=None), "s1", {"title": "T"}, "e1"],
    model_state=[[]], provenance_meta=[{}], session_modes=[{"current_mode_id": "ask"}],
    session_config_options=[[]])
add("_session_fields", [st(_home="/h", _modes={"s1": "ask"}, _gateway=True, _conn=None), "s1", {"title": "  "}, "e1"],
    model_state=[[]], provenance_meta=[{}], session_modes=[{"current_mode_id": "ask"}],
    session_config_options=[[]])
add("_session_fields", [st(_home="/h", _modes={}, _gateway=True, _conn=True), "s1", {"title": "T"}, "e1"],
    model_state=[[]], provenance_meta=[{}], session_modes=[{"current_mode_id": "ask"}],
    session_config_options=[[]], create_task=[None])
add("_session_fields", [st(_home="/h", _modes={}, _gateway=True), "s1", {}, "e1"],
    model_state=[VE()])
add("_session_fields", [st(_home="/h", _modes={}, _gateway=True), "s1", {}, "e1"],
    model_state=[[]], provenance_meta=[VE()])
add("_session_fields", [st(_home="/h", _modes={}, _gateway=True), "s1", {}, "e1"],
    model_state=[[]], provenance_meta=[{}], session_modes=[VE()])

# 41 _teardown
add("_teardown", [st(_event_task=None, _connection=None)], )
add("_teardown", [st(_event_task="T", _connection="C", _streamed={"a": 1}, _segments={"a": 1},
    _message_ids={"a": 1}, _cut={"a": 1}, _failure="F", _link=0)],
    teardown_cancel=[None], connection___aexit__=[None])
add("_teardown", [st(_event_task="T", _connection=None)], teardown_cancel=[VE()])
add("_teardown", [st(_event_task=None, _connection="C")], connection___aexit__=[VE()])
add("_teardown", [st(_event_task=None, _connection="C")], connection___aexit__=[SX()])
add("_teardown", [st(_event_task="T", _connection="C")], teardown_cancel=[None], connection___aexit__=[None])

# 42 _notify_unknown
add("_notify_unknown", [st(_snapshots={"s1": {"pending": [{"status": "unknown"}]}}, _conn=True), "s1"],
    has_unknown=[True], unknown_recovery=["unknown_execution: lost"])
add("_notify_unknown", [st(_snapshots={"s1": {}}, _conn=True), "s1"], has_unknown=[False])
add("_notify_unknown", [st(_snapshots={"s1": {"pending": [{"status": "unknown"}]}}, _conn=None), "s1"],
    has_unknown=[True])
add("_notify_unknown", [st(_snapshots={}, _conn=True), "s1"], has_unknown=[VE()])
add("_notify_unknown", [st(_conn=True), "s1"], has_unknown=[True], unknown_recovery=[VE()])
add("_notify_unknown", [st(_snapshots={"s1": {"pending": [{"status": "unknown"}]}}, _conn=True), "s1"],
    has_unknown=[True], unknown_recovery=[SX()])

# 31 (placeholder: all 31 listed above). Verify count in emit().


if __name__ == "__main__":
    emit()
