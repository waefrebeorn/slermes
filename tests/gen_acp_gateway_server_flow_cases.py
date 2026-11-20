#!/usr/bin/env python3
"""Generate tests/fixtures/acp_gateway_server_flow.jsonl.

Every case drives the LIVE def (via oracle_acp_gateway_server_flow.drive) under
a scripted 'world', captures the ordered seam trace, and stores the per-name
queue of recorded seam RETURNS as the fixture's `seams`. Replaying those scripts
in the C harness and in the oracle reproduces identical seam behaviour, so the
differential compares the port's own control flow / string assembly.

Each function gets >= 6 cases. Raise scripts use the full builtin exception
hierarchy; BaseException-only kinds (SystemExit / CancelledError) exercise the
real passthrough through `except Exception` boundaries.
"""
import importlib.util
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location(
    "oracle_acp_gateway_server_flow", HERE / "oracle_acp_gateway_server_flow.py")
oracle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(oracle)
oracle.load_ns()

CASES = []
R = lambda msg="boom": {"__raise__": "RuntimeError", "message": msg}
SX = lambda msg="die": {"__raise__": "SystemExit", "message": msg}
CE = lambda msg="cancelled": {"__raise__": "CancelledError", "message": msg}
VE = lambda msg="bad": {"__raise__": "ValueError", "message": msg}
GCE = lambda msg="x": {"__raise__": "GatewayClientError", "message": msg}


def add(func, argv=None, kw=None, **world):
    alias = {
        "os_environ_get": "os.environ.get", "gateway_reader": "gateway.reader",
        "logger_debug": "logger.debug", "logger_info": "logger.info",
        "conn_session_update": "conn.session_update", "changed_wait_for": "changed.wait_for",
        "answer_approval": "answer.approval", "answer_clarify": "answer.clarify",
    }
    world = {alias.get(k, k): v for k, v in world.items()}
    CASES.append((func, list(argv or []), dict(kw or {}), dict(world)))


def state(**kw):
    return kw


def emit():
    out = []
    for func, argv, kw, world in CASES:
        r = oracle.drive(func, argv, kw, world)
        seams = {}
        for ev in r["trace"]:
            seams.setdefault(ev["ev"], []).append(ev["ret"])
        out.append({"func": func, "in": {"argv": argv, "kw": kw, "seams": seams}})
    counts = {}
    for c in out:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    bad = {k: v for k, v in counts.items() if v < 6}
    if bad:
        raise SystemExit("too few cases: %r" % bad)
    fx = HERE / "fixtures" / "acp_gateway_server_flow.jsonl"
    fx.parent.mkdir(exist_ok=True)
    with open(fx, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(json.dumps(c, ensure_ascii=False, separators=(",", ":")) for c in out) + "\n")
    print("wrote %d cases across %d funcs -> %s" % (len(out), len(counts), fx))


# ---------------- 1 _slash_command ----------------
add("_slash_command", ["/foo"], resolve_command=[{"name": "foo"}], ROUTES=[[]])
add("_slash_command", ["/foo"], resolve_command=[None], ROUTES=[["foo"]])
add("_slash_command", ["/foo"], resolve_command=[None], ROUTES=[[]])
add("_slash_command", ["plain text"])
add("_slash_command", ["/etc/hosts"])
add("_slash_command", ["/"])
add("_slash_command", ["/  "])
add("_slash_command", ["/x"], resolve_command=[SX()])

# ---------------- 2 _tools_changed_notice ----------------
add("_tools_changed_notice", [{"mcp_servers": ["a", "b"]}])
add("_tools_changed_notice", [{"mcp_servers": []}])
add("_tools_changed_notice", [{}])
add("_tools_changed_notice", [{"mcp_servers": None}])
add("_tools_changed_notice", [{"mcp_servers": ["x"]}])
add("_tools_changed_notice", [{"mcp_servers": ["a", "b", "c"]}])

# ---------------- 3 _bypass_route ----------------
add("_bypass_route", os_environ_get=["m", "p"])
add("_bypass_route", os_environ_get=[None, "m2", "p"])
add("_bypass_route", os_environ_get=["  spaced  ", "  pv  "])
add("_bypass_route", os_environ_get=[None, None, "p"])
add("_bypass_route", os_environ_get=["m", ""])
add("_bypass_route", os_environ_get=["m", "   "])
add("_bypass_route", os_environ_get=[SX()])

# ---------------- 4 _editor_id ----------------
add("_editor_id", [state(_aliases={"h1": "s1", "h2": "s2"}), "s2"])
add("_editor_id", [state(_aliases={"h1": "s1"}), "s1"])
add("_editor_id", [state(_aliases={"h1": "s1"}), "zzz"])
add("_editor_id", [state(_aliases={}), "s1"])
add("_editor_id", [state(_aliases={"a": "x", "b": "y", "c": "x"}), "x"])
add("_editor_id", [state(_aliases={"h": "s"}), ""])

# ---------------- 5 _moved ----------------
add("_moved", [{"cwd": "/a"}, "/b"],
    _normalize_cwd_for_compare=["A", "B"], _translate_acp_cwd=["/b"])
add("_moved", [{"cwd": "/a"}, "/a"],
    _normalize_cwd_for_compare=["A", "A"], _translate_acp_cwd=["/a"])
add("_moved", [{}, "/b"])
add("_moved", [{"cwd": None}, "/b"],
    _normalize_cwd_for_compare=[None, "B"], _translate_acp_cwd=["/b"])
add("_moved", [{"cwd": "/a"}, "/b"],
    _normalize_cwd_for_compare=["A", "B"], _translate_acp_cwd=[SX()])
add("_moved", [{"cwd": "/a"}, "/b"],
    _normalize_cwd_for_compare=[VE()], _translate_acp_cwd=["/b"])

# ---------------- 6 _transport_closed ----------------
add("_transport_closed", [state(_gateway=True)],
    gateway_reader=[{"is_future": True, "done": True}])
add("_transport_closed", [state(_gateway=True)],
    gateway_reader=[{"is_future": True, "done": False}])
add("_transport_closed", [state(_gateway=True)],
    gateway_reader=[{"is_future": False, "done": True}])
add("_transport_closed", [state(_gateway=True)], gateway_reader=[[None]])
add("_transport_closed", [state(_gateway=True)], gateway_reader=[{}])
add("_transport_closed", [state(_gateway=True)],
    gateway_reader=[{"is_future": True}])

# ---------------- 7 _held_session ----------------
add("_held_session", [state(_aliases={"h": "s1"}, _snapshots={"s1": {}}), "h"])
add("_held_session", [state(_aliases={}, _snapshots={"s1": {}}), "s1"])
add("_held_session", [state(_aliases={}, _snapshots={}), "s1"])
add("_held_session", [state(_aliases={"h": "gone"}, _snapshots={"s1": {}}), "h"])
add("_held_session", [state(_aliases={"a": "x", "b": "y"}, _snapshots={"y": {}}), "a"])
add("_held_session", [state(_aliases={}, _snapshots={"": {}}), ""])

# ---------------- 8 _has_unknown ----------------
add("_has_unknown", [state(_snapshots={"s1": {"pending": [{"status": "unknown"}]}}), "s1"])
add("_has_unknown", [state(_snapshots={"s1": {"pending": [{"status": "queued"}, {"status": "unknown"}]}}), "s1"])
add("_has_unknown", [state(_snapshots={"s1": {"pending": [{"status": "queued"}]}}), "s1"])
add("_has_unknown", [state(_snapshots={"s1": {}}), "s1"])
add("_has_unknown", [state(_snapshots={"s1": {"pending": []}}), "s1"])
add("_has_unknown", [state(_snapshots={}), "missing"])

# ---------------- 9 _pending_event_is_new ----------------
F = state(_snapshots={"s1": {"_pending_watermark": [1, 10]}})
add("_pending_event_is_new", [F, "s1", {"seq": 5, "replay_epoch": 1}])
add("_pending_event_is_new", [F, "s1", {"seq": 11, "replay_epoch": 1}])
add("_pending_event_is_new", [F, "s1", {"seq": 5, "replay_epoch": 2}])
add("_pending_event_is_new", [state(_snapshots={"s1": {}}), "s1", {"seq": 5, "replay_epoch": 1}])
add("_pending_event_is_new", [F, "s1", {"replay_epoch": 1}])
add("_pending_event_is_new", [state(_snapshots={}), "s1", {"seq": 1, "replay_epoch": 1}])

# ---------------- 10 _blocked_admission ----------------
add("_blocked_admission", [state(_snapshots={"s1": {"pending": [{"admission_id": "a1", "status": "unknown"}]}}), "s1", "a1"])
add("_blocked_admission", [state(_snapshots={"s1": {"pending": [{"admission_id": "a1", "status": "queued"}, {"admission_id": "a2", "status": "unknown"}]}}), "s1", "a1"])
add("_blocked_admission", [state(_snapshots={"s1": {"pending": [{"admission_id": "a1", "status": "terminal"}, {"admission_id": "a2", "status": "unknown"}]}}), "s1", "a1"])
add("_blocked_admission", [state(_snapshots={"s1": {"pending": [{"admission_id": "a1", "status": "queued"}]}}), "s1", "a1"])
add("_blocked_admission", [state(_snapshots={"s1": {"pending": [{"admission_id": "a1", "status": "unknown"}]}}), "s1", "b9"])
add("_blocked_admission", [state(_snapshots={"s1": {}}), "s1", "a1"])

# ---------------- 11 _unknown_recovery ----------------
add("_unknown_recovery", [state(_snapshots={"s1": {"pending": [{"admission_id": "a1", "status": "unknown"}]}}, _home="/h"), "s1", "a1"], profile_name_for_home=["default"])
add("_unknown_recovery", [state(_snapshots={"s1": {"pending": [{"admission_id": "a1", "status": "unknown"}, {"admission_id": "a2", "status": "unknown"}]}}, _home="/h"), "s1", "a1"], profile_name_for_home=["p"])
add("_unknown_recovery", [state(_snapshots={}, _home="/h"), "s1", "a1"], profile_name_for_home=["default"])
add("_unknown_recovery", [state(_snapshots={"s1": {"pending": [{"admission_id": "a1", "status": "unknown"}]}}, _home="/h"), "s1", "a1"], {"submitted": False}, profile_name_for_home=["default"])
add("_unknown_recovery", [state(_snapshots={}, _home="/h"), "s1", "a1"], profile_name_for_home=[None])
add("_unknown_recovery", [state(_snapshots={"s1": {"pending": [{"admission_id": "a1", "status": "unknown"}]}}, _home="/h"), "s1", "a1"], profile_name_for_home=[SX()])

# ---------------- 12 _request_live_reasoning ----------------
add("_request_live_reasoning", [state(_conn=None), "CLIENT", "s1"], rpc=[{"ok": True}])
add("_request_live_reasoning", [state(_conn=None), "CLIENT", "s1"], rpc=[VE()], logger_debug=[None])
add("_request_live_reasoning", [state(_conn=None), "CLIENT", "s1"], rpc=[GCE()], logger_debug=[None])
add("_request_live_reasoning", [state(_conn=None), "CLIENT", "s1"], rpc=[SX()])
add("_request_live_reasoning", [state(_conn=None), "CLIENT", "s1"], rpc=[CE()])
add("_request_live_reasoning", [state(_conn=None), "CLIENT", "s1"], rpc=[R()], logger_debug=[None])

# ---------------- 13 _after_attach ----------------
ST = state(_conn=True, _usage={})
add("_after_attach", [ST, "s1", "e1"], sleep=[0], available_commands_update=[{"u": 1}], conn_session_update=[None, None], client=["CLIENT"], rpc=[{"stats": 1}], usage_update=[{"up": 1}])
add("_after_attach", [state(_conn=True, _usage={}), "s1", "e1"], sleep=[0], available_commands_update=[SX()])
add("_after_attach", [state(_conn=True, _usage={}), "s1", "e1"], sleep=[0], available_commands_update=[{"u": 1}], conn_session_update=[VE()], logger_debug=[None])
add("_after_attach", [state(_conn=True, _usage={}), "s1", "e1"], sleep=[0], available_commands_update=[{"u": 1}], conn_session_update=[None], client=[SX()])
add("_after_attach", [state(_conn=True, _usage={}), "s1", "e1"], sleep=[0], available_commands_update=[{"u": 1}], conn_session_update=[None], client=["CLIENT"], rpc=[VE()], logger_debug=[None])
add("_after_attach", [state(_conn=True, _usage={}), "s1", "e1"], sleep=[0], available_commands_update=[{"u": 1}], conn_session_update=[None], client=["CLIENT"], rpc=[{"stats": 1}], usage_update=[[None]])

# ---------------- 14 _admission_from_receipt ----------------
BASE = state(_conn=True, _pending_cancels=[], _admissions={})
add("_admission_from_receipt", [BASE, "CLIENT", "h1", "s1", {"admission_id": "a1", "status": "started"}])
add("_admission_from_receipt", [state(_conn=True, _pending_cancels=[], _admissions={}), "CLIENT", "h1", "s1", {"voice_stopped": True}], t=["STOP"], update_agent_message_text=["txt"], conn_session_update=[None])
add("_admission_from_receipt", [state(_conn=None, _pending_cancels=[], _admissions={}), "CLIENT", "h1", "s1", {"voice_stopped": True}])
add("_admission_from_receipt", [state(_conn=True, _pending_cancels=[], _admissions={}), "CLIENT", "h1", "s1", {"admission_id": "a1", "status": "terminal"}], settled_receipt=[None])
add("_admission_from_receipt", [state(_conn=True, _pending_cancels=[], _admissions={}, _snapshots={}, _home="/h"), "CLIENT", "h1", "s1", {"admission_id": "a1", "status": "unknown"}], profile_name_for_home=["default"])
add("_admission_from_receipt", [state(_conn=True, _pending_cancels=[], _admissions={}), "CLIENT", "h1", "s1", {"admission_id": "a1", "voice_stopped": 1, "status": "started"}])

# ---------------- 15 _after_ack ----------------
ACK = state(_resumes={"s1": 1}, _adopted={"s1": 2}, _failure=None, _link=0, _gateway={"events": "EV"})
add("_after_ack", [ACK, "s1", "a1", 0, 1])
add("_after_ack", [state(_resumes={"s1": 0}, _adopted={"s1": 2}, _failure=None, _link=0, _gateway={"events": "EV"}), "s1", "a1", 0, 1], changed_wait_for=[None], put_behind=[None])
add("_after_ack", [state(_resumes={"s1": 0}, _adopted={"s1": 0}, _failure=None, _link=0, _gateway={"events": "EV"}), "s1", "a1", 0, 0], changed_wait_for=[None])
add("_after_ack", [state(_resumes={"s1": 0}, _adopted={"s1": 2}, _failure=None, _link=0, _gateway={}), "s1", "a1", 0, 0], changed_wait_for=[SX()])
add("_after_ack", [state(_resumes={"s1": 0}, _adopted={"s1": 2}, _failure=None, _link=0, _gateway={}), "s1", "a1", 0, 0], changed_wait_for=[VE()])
add("_after_ack", [state(_resumes={"s1": 0}, _adopted={"s1": 2}, _failure=None, _link=0, _gateway={"events": "EV"}), "s1", "a1", 0, 0], changed_wait_for=[None], put_behind=[R()])

# ---------------- 16 _project_title ----------------
add("_project_title", [state(_titles={}, _conn=True), "s1", "e1", "new"], title_update=[{"tu": 1}], conn_session_update=[None])
add("_project_title", [state(_titles={"s1": "same"}, _conn=True), "s1", "e1", "same"])
add("_project_title", [state(_titles={}, _conn=True), "s1", "e1", "   "])
add("_project_title", [state(_titles={}, _conn=True), "s1", "e1", 42])
add("_project_title", [state(_titles={}, _conn=None), "s1", "e1", "new"])
add("_project_title", [state(_titles={}, _conn=True), "s1", "e1", "  padded  "], title_update=[{"tu": 2}], conn_session_update=[R()])

# ---------------- 17 _project_usage ----------------
add("_project_usage", [state(_usage={}, _conn=True), "s1", "e1", {"a": 1}], usage_update=[{"up": 1}], conn_session_update=[None])
add("_project_usage", [state(_usage={}, _conn=True), "s1", "e1", {"a": 1}], usage_update=[[None]])
add("_project_usage", [state(_usage={}, _conn=None), "s1", "e1", {"a": 1}])
add("_project_usage", [state(_usage={}, _conn=True), "s1", "e1", {}], usage_update=[SX()])
add("_project_usage", [state(_usage={}, _conn=True), "s1", "e1", {"b": 2}], usage_update=[{"up": 2}], conn_session_update=[VE()])

# only 5 above -> add one
add("_project_usage", [state(_usage={}, _conn=True), "s1", "e1", None], usage_update=[{"up": 3}], conn_session_update=[None])

# ---------------- 18 _permission ----------------
P = {"kind": "approval", "prompt_id": "p1", "execution_generation": 1}
G = {"kind": "clarify", "prompt_id": "p1", "execution_generation": 1}
add("_permission", [state(_conn=True, _permissions={}), "s1", P], answer_approval=["CORO"], create_task=[{"t": 1}])
add("_permission", [state(_conn=True, _permissions={}), "s1", G], answer_clarify=["CORO"], create_task=[{"t": 1}])
add("_permission", [state(_conn=True, _permissions={}), "s1", {"kind": "other", "prompt_id": "p1", "execution_generation": 1}])
add("_permission", [state(_conn=None, _permissions={}), "s1", P])
add("_permission", [state(_conn=True, _permissions={"s1|p1|1": 0}), "s1", {"kind": "approval", "prompt_id": "p1", "execution_generation": "1"}])
add("_permission", [state(_conn=True, _permissions={}), "s1", P], answer_approval=[SX()])
add("_permission", [state(_conn=True, _permissions={}), "s1", P], answer_approval=["CORO"], create_task=[VE()])

# ---------------- 19 _project_config ----------------
CFG1 = state(_configs={"s1": {"model": "m", "mode": "ask"}}, _modes={}, _conn=True)
add("_project_config", [CFG1, "s1", "e1", {"model": "m"}], model_state=[["m"]], session_config_options=[["opt"]], ConfigOptionUpdate=[{"c": 1}], conn_session_update=[None])
add("_project_config", [state(_configs={}, _modes={}, _conn=True), "s1", "e1", {}])
add("_project_config", [state(_configs={}, _modes={}, _conn=True), "s1", "e1", {"model": "m"}], current_mode=["ask"])
add("_project_config", [state(_configs={"s1": {"model": "m", "mode": "ask"}}, _modes={}, _conn=None), "s1", "e1", {"model": "m"}])
add("_project_config", [state(_configs={"s1": {"model": "m", "mode": "ask"}}, _modes={}, _conn=True), "s1", "e1", {"service_tier": "fast"}], model_state=[["m"]], session_config_options=[["opt"]], ConfigOptionUpdate=[{"c": 1}], conn_session_update=[None])
add("_project_config", [state(_configs={"s1": {"model": "m", "mode": "ask"}}, _modes={}, _conn=True), "s1", "e1", {"model": "m"}], model_state=[SX()])
add("_project_config", [state(_configs={}, _modes={"s1": "ask"}, _conn=True), "s1", "e1", {"model": "m"}], model_state=[["m"]], session_config_options=[VE()])

# ---------------- 20 _answer_permission ----------------
PROMPT = {"choices": ["once"], "command": "cmd", "description": "d", "prompt_id": "p1", "execution_generation": 1}
OPT = [{"option_id": "allow_once", "name": "Allow once"}]
add("_answer_permission", [state(_conn=True, _gateway=True), "s1", PROMPT], build_permission_options=[OPT], build_permission_tool_call=[{"tc": 1}], request_permission=[{"outcome": {"allowed_outcome": True, "option_id": "allow_once"}}], close_card=[None], rpc=[True])
add("_answer_permission", [state(_conn=True, _gateway=True), "s1", PROMPT], build_permission_options=[OPT], build_permission_tool_call=[{"tc": 1}], request_permission=[{"outcome": {"allowed_outcome": False, "option_id": "deny"}}], close_card=[None])
add("_answer_permission", [state(_conn=True, _gateway=True), "s1", PROMPT], build_permission_options=[OPT], build_permission_tool_call=[{"tc": 1}], request_permission=[{"outcome": {"allowed_outcome": True, "option_id": "deny"}}], close_card=[None])
add("_answer_permission", [state(_conn=True, _gateway=True), "s1", {**PROMPT, "edit": {"path": "f"}}], build_permission_options=[OPT], EditProposal=[{"path": "f"}], build_acp_edit_tool_call=[{"tc": 2}], request_permission=[{"outcome": {"allowed_outcome": True, "option_id": "allow_once"}}], close_card=[None], rpc=[True])
add("_answer_permission", [state(_conn=True, _gateway=True), "s1", PROMPT], build_permission_options=[OPT], build_permission_tool_call=[{"tc": 1}], request_permission=[VE()], logger_info=[None])
add("_answer_permission", [state(_conn=True, _gateway=True), "s1", PROMPT], build_permission_options=[OPT], build_permission_tool_call=[{"tc": 1}], request_permission=[{"outcome": {"allowed_outcome": True, "option_id": "allow_once"}}], close_card=[None], rpc=[SX()])
add("_answer_permission", [state(_conn=True, _gateway=True), "s1", PROMPT], build_permission_options=[SX()])


if __name__ == "__main__":
    emit()
