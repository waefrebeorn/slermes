#!/usr/bin/env python3
"""Generate tests/fixtures/relay_metrics_flow2.jsonl — the scripted-seam case
corpus for the relay_metrics_flow2 FLOW port (the UNCLAIMED fourth-wave remainder
of hermes_cli/observability/relay_shared_metrics.py). >=5 cases per ported func,
covering happy paths, branch gates, scripted Exception (raised through), scripted
BaseException (passthrough), state mutations, and fail-closed missing scripts.

Each case: {"func": <live_name>, "in": {...args..., "io": {seam: script},
"state": {...mutable carrier...}}}. A bare JSON array script is a per-call
queue; a list RESULT is wrapped {"__script__": true, "ret": [...]}; a raise is
{"__raise__": kind, "message": msg}. Deterministic (no randomness).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "relay_metrics_flow2.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def raise_(kind, msg="boom"):
    return {"__raise__": kind, "message": msg}


def seq(*vals):
    return list(vals)


def listret(arr):
    return {"__script__": True, "ret": arr}


def sids(lst):
    """sort identity 3-arrays by the Python str(list) key (oracle norm order)."""
    return sorted(lst, key=str)


def sstr(lst):
    return sorted(lst)


ROUTE = {"provider": "p", "model": "m"}
BREAK_ROW = listret(["hermes.cache_break", {"c": "1"}])


def sess_obs(**over):
    d = {"session_id": "s1", "last_turn_ns": 9, "model_state": {"last_route": ROUTE},
         "route_run": {"ref": "RR"}, "efficiency": {"spent": []}}
    d.update(over)
    return d


def task_obs(**over):
    d = {"task_id": "t1", "model_route": None, "selected_route": None,
         "cost": {"user_turn": False}, "start_fields": {},
         "model_call_ids": [], "tool_call_ids": [], "unidentified_tool_calls": 0}
    d.update(over)
    return d


# ── _settle_connector_batch ──
SCB_SESS = {"session_id": "s1", "tool_calls": []}
SCB_TASK = {"task_id": "t1", "completed_tool_call_ids": []}
add("_settle_connector_batch", wrapper=["r1", "u1", "c1"], event={"x": 1},
    state={"session": SCB_SESS, "task": SCB_TASK},
    io={"connectors.is_connector_batch": False})
add("_settle_connector_batch", wrapper=["r1", "u1", "c1"], event={},
    state={"session": {"tool_calls": []}, "task": {"task_id": "t1", "completed_tool_call_ids": []}},
    io={"connectors.is_connector_batch": True, "connectors.batch_entry_ran": False})
add("_settle_connector_batch", wrapper=["r1", "u1", "c1"], event={},
    state={"session": {"tool_calls": []}, "task": {"task_id": "t1", "completed_tool_call_ids": []}},
    io={"connectors.is_connector_batch": True, "connectors.batch_entry_ran": True})
add("_settle_connector_batch", wrapper=["r1", "u1", "c1"], event={"status": "ok"},
    state={"session": {"tool_calls": [[["t1", "r1", "u1", "c1"], {"call": 1}]]},
           "task": {"task_id": "t1", "completed_tool_call_ids": []}},
    io={"connectors.is_connector_batch": True, "connectors.is_batch_entry": True,
        "rt._finish_tool_call": None})
add("_settle_connector_batch", wrapper=["r1", "u1", "c1"], event={},
    state={"session": {"tool_calls": [[["t2", "r1", "u1", "c1"], {}]]},
           "task": {"task_id": "t1", "completed_tool_call_ids": []}},
    io={"connectors.is_connector_batch": True, "connectors.batch_entry_ran": False})
add("_settle_connector_batch", wrapper=["r1", "u1", "c1"], event={},
    state={"session": {"tool_calls": [[["t1", "r1", "u1", "c1"], {}]]},
           "task": {"task_id": "t1", "completed_tool_call_ids": []}},
    io={"connectors.is_connector_batch": True, "connectors.is_batch_entry": raise_("ValueError")})
add("_settle_connector_batch", wrapper=["r1", "u1", "c1"], event={},
    state={"session": {"tool_calls": [[["t1", "r1", "u1", "c1"], {}]]},
           "task": {"task_id": "t1", "completed_tool_call_ids": []}},
    io={"connectors.is_connector_batch": True, "connectors.is_batch_entry": True,
        "rt._finish_tool_call": raise_("SystemExit")})
add("_settle_connector_batch", wrapper=["r1", "u1", "c1"], event={},
    state={"session": SCB_SESS, "task": SCB_TASK}, io={})

# ── _observe_model_turn ──
add("_observe_model_turn", fields={"outcome": "success", "end_reason": "completed"},
    state={"session": sess_obs(), "task": task_obs()},
    io={"model_state.observe_turn": None})
add("_observe_model_turn", fields={"outcome": "success", "end_reason": "completed"},
    state={"session": sess_obs(),
           "task": task_obs(model_route=ROUTE, cost={"user_turn": True}, model_call_ids=["r1"])},
    io={"model_state.observe_turn": None, "engagement.engaged_turn": False,
        "model.attended": True, "efficiency.finish_turn": listret([["hermes.x", {"a": "1"}]]),
        "rt._emit_rows": None})
add("_observe_model_turn", fields={"outcome": "success", "end_reason": "completed"},
    state={"session": sess_obs(), "task": task_obs(model_route=ROUTE, cost={"user_turn": False})},
    io={"model_state.observe_turn": None, "engagement.engaged_turn": True, "rt._guarded": None})
add("_observe_model_turn", fields={"outcome": "failed", "end_reason": "user_cancelled"},
    state={"session": sess_obs(), "task": task_obs()},
    io={"model_state.observe_turn": None, "model.attended": True,
        "model.friction_fields": {"f": "1"}, "rt._guarded": None})
add("_observe_model_turn", fields={"outcome": "success", "end_reason": "completed"},
    state={"session": sess_obs(),
           "task": task_obs(selected_route=ROUTE, cost={"user_turn": True})},
    io={"model_state.observe_turn": None, "rt._with_route_run": None, "model.attended": False})
add("_observe_model_turn", fields={"outcome": "success", "end_reason": "completed"},
    state={"session": sess_obs(), "task": task_obs()},
    io={"model_state.observe_turn": raise_("KeyboardInterrupt")})
add("_observe_model_turn", fields={"end_reason": "completed"},
    state={"session": sess_obs(), "task": task_obs()},
    io={"model_state.observe_turn": None})
add("_observe_model_turn", fields={"outcome": "success", "end_reason": "completed"},
    state={"session": sess_obs(), "task": task_obs()}, io={})

# ── _observe_turn_call ──
OTC_SESS = {"session_id": "s1", "efficiency": {}}
OTC_TASK = {"cost": {"user_turn": True}}
OTC_MC = {"fields": ROUTE, "started_ns": 5}
OTC_IO = {"cost.add_call": None, "rt._cold_key": "K", "rt._consume_cold": False,
          "monotonic_ns": 100, "efficiency.observe_cache": None}
add("_observe_turn_call", event={"usage": {"tokens": 1}},
    state={"session": OTC_SESS, "task": None, "model_call": OTC_MC}, io={})
add("_observe_turn_call", event={"usage": {}},
    state={"session": OTC_SESS, "task": {"cost": {"user_turn": False}}, "model_call": OTC_MC}, io={})
add("_observe_turn_call", event={"usage": {"tokens": 1}},
    state={"session": OTC_SESS, "task": OTC_TASK, "model_call": OTC_MC}, io=OTC_IO)
add("_observe_turn_call", event={"usage": {}},
    state={"session": OTC_SESS, "task": OTC_TASK, "model_call": OTC_MC},
    io=dict(OTC_IO, **{"efficiency.observe_cache": "cold",
                       "eff.cache_break_row": BREAK_ROW, "rt._emit_rows": None}))
add("_observe_turn_call", event={},
    state={"session": OTC_SESS, "task": OTC_TASK, "model_call": OTC_MC},
    io=dict(OTC_IO, **{"efficiency.observe_cache": raise_("ValueError")}))
add("_observe_turn_call", event={},
    state={"session": OTC_SESS, "task": OTC_TASK, "model_call": OTC_MC},
    io={"cost.add_call": None, "rt._cold_key": raise_("SystemExit")})
add("_observe_turn_call", event={},
    state={"session": OTC_SESS, "task": OTC_TASK, "model_call": None}, io={})
add("_observe_turn_call", event={},
    state={"session": OTC_SESS, "task": OTC_TASK, "model_call": OTC_MC}, io={})

# ── record_known_cache_break ──
add("record_known_cache_break", cause="prompt_edit", route=ROUTE, session_id="s1",
    io={"rt._cold_key": "K", "rt._announce_cold": True,
        "eff.cache_break_row": BREAK_ROW, "rt._emit_rows": None})
add("record_known_cache_break", cause="prompt_edit", route=ROUTE, session_id="s1",
    io={"rt._cold_key": "K", "rt._announce_cold": False})
add("record_known_cache_break", cause="x", route={}, session_id="s1",
    io={"rt._cold_key": "K", "rt._announce_cold": raise_("GeneratorExit")})
add("record_known_cache_break", cause="x", route=ROUTE, session_id="s1",
    io={"rt._cold_key": "K", "rt._announce_cold": True, "eff.cache_break_row": raise_("ValueError")})
add("record_known_cache_break", cause="x", route=ROUTE, session_id="s1", io={})
add("record_known_cache_break", cause="x", route=ROUTE, session_id="s1",
    io={"rt._cold_key": "K", "rt._announce_cold": True, "eff.cache_break_row": None,
        "rt._emit_rows": None})

# ── record_session_tools ──
RST_SESS = {"session_id": "s1", "closing": False,
            "efficiency": {"tools_key": [0, 0, 0], "tools": {"__tools__": 1}}}
RST_AGENT = {"tools": {"t": 1}, "provider": "p", "model": "m"}
RST_MARKER = {"__session_ref__": "session"}
add("record_session_tools", session_id="", agent=RST_AGENT, tools_for_api=["a"],
    state={"session": RST_SESS}, io={})
add("record_session_tools", session_id="s1", agent=RST_AGENT, tools_for_api=["a"],
    state={"session": RST_SESS}, io={"rt._session": None})
add("record_session_tools", session_id="s1", agent=RST_AGENT, tools_for_api=["a"],
    state={"session": dict(RST_SESS, closing=True)},
    io={"rt._session": RST_MARKER, "id": seq(11, 22)})
add("record_session_tools", session_id="s1", agent=RST_AGENT, tools_for_api=["a"],
    state={"session": {"session_id": "s1", "closing": False,
                       "efficiency": {"tools_key": [11, 22, 1], "tools": {}}}},
    io={"rt._session": RST_MARKER, "id": seq(11, 22)})
add("record_session_tools", session_id="s1", agent=RST_AGENT, tools_for_api=["a"],
    state={"session": RST_SESS},
    io={"rt._session": RST_MARKER, "id": seq(11, 22),
        "eff.tool_snapshot": listret([["a"], [], 1, 2]),
        "efficiency.tools.observe": True, "model.model_route": ROUTE,
        "rt._cold_key": "K", "rt._announce_cold": True,
        "eff.cache_break_row": BREAK_ROW, "rt._emit_rows": None})
add("record_session_tools", session_id="s1", agent=RST_AGENT, tools_for_api=["a"],
    state={"session": RST_SESS},
    io={"rt._session": RST_MARKER, "id": seq(11, 22),
        "eff.tool_snapshot": listret([["a"], [], 1, 2]),
        "efficiency.tools.observe": False, "model.model_route": ROUTE})
add("record_session_tools", session_id="s1", agent=RST_AGENT, tools_for_api=None,
    state={"session": RST_SESS}, io={"rt._session": RST_MARKER, "id": seq(11, 22)})
add("record_session_tools", session_id="s1", agent=RST_AGENT, tools_for_api=["a"],
    state={"session": RST_SESS},
    io={"rt._session": RST_MARKER, "id": seq(11, 22), "eff.tool_snapshot": raise_("CancelledError")})
add("record_session_tools", session_id="s1", agent=RST_AGENT, tools_for_api=["a"],
    state={"session": RST_SESS}, io={})

# ── rotate_segment ──
def old_sess(**over):
    d = {"session_id": "o1", "route_run": {"ref": "RR"}, "efficiency": {"spent": ["t1"]},
         "connectors": {"chain": 1}, "closing": False, "tasks": {}}
    d.update(over)
    return d


def lineage(open_ids):
    return {"open": sstr(open_ids), "peak": None, "tally": None, "tools": None,
            "run": None, "spent": None, "connectors": None}


ROT_CTOR = {"model.ModelSessionState": {"peak": 1}, "eff.ToolUsage": {"tools": 1},
            "engagement.SessionTally": {"tally": 1}}
add("rotate_segment", old_id="o1", new_id="n1",
    state={"_lineages": {}, "_lineage_of": {}}, io={"rt._session": None})
add("rotate_segment", old_id="o1", new_id="n1",
    state={"_lineages": {}, "_lineage_of": {}, "old": old_sess(closing=True)},
    io=dict({"rt._session": seq({"__session_ref__": "old"}, None)}, **ROT_CTOR))
add("rotate_segment", old_id="o1", new_id="n1",
    state={"_lineages": {"root": lineage(["o1"])}, "_lineage_of": {"o1": "root"},
           "old": old_sess()},
    io={"rt._session": seq({"__session_ref__": "old"}, None), "rt.close_session": None})
add("rotate_segment", old_id="o1", new_id="n1",
    state={"_lineages": {"root": lineage(["o1"])}, "_lineage_of": {"o1": "root"},
           "old": old_sess(), "new": {"session_id": "n1"}},
    io={"rt._session": seq({"__session_ref__": "old"}, {"__session_ref__": "new"}),
        "rt._share_spent": None, "rt.close_session": None})
add("rotate_segment", old_id="o1", new_id="n1",
    state={"_lineages": {"root": lineage(["o1"])}, "_lineage_of": {"o1": "root"},
           "old": old_sess(tasks={"t1": {}})},
    io={"rt._session": seq({"__session_ref__": "old"}, None)})
add("rotate_segment", old_id="o1", new_id="n1",
    state={"_lineages": {"root": lineage(["o1"])}, "_lineage_of": {"o1": "root"},
           "old": old_sess()},
    io={"rt._session": seq({"__session_ref__": "old"}, None), "rt.close_session": raise_("SystemExit")})
add("rotate_segment", old_id="o1", new_id="n1",
    state={"_lineages": {}, "_lineage_of": {"o1": "root"}, "old": old_sess()},
    io={"rt._session": {"__session_ref__": "old"}})
add("rotate_segment", old_id="o1", new_id="n1",
    state={"_lineages": {}, "_lineage_of": {}}, io={})
# overflow: 513 lineages, the first (L000) overflows past _MAX_LINEAGES=512
_big_lineages = {("L%03d" % i): lineage([]) for i in range(513)}
add("rotate_segment", old_id="o1", new_id="n1",
    state={"_lineages": _big_lineages, "_lineage_of": {"o1": "L000"}, "old": old_sess()},
    io={"rt._session": seq({"__session_ref__": "old"}, None),
        "rt.close_session": None, "rt._flush_lineage": None})

# ── _emit_session_summary ──
def sum_sess(**over):
    d = {"session_id": "s1", "start_fields": {"entrypoint": "cli"}, "turns": 2,
         "failed_turns": 0, "last_outcome": "success", "first_turn_ns": 1, "last_turn_ns": 2,
         "api_calls": 1, "tool_call_total": 1, "replies": 1,
         "efficiency": {"tools": {"__tools__": 1}}, "model_state": {"last_route": ROUTE},
         "retiring": False}
    d.update(over)
    return d


MERGED = listret([{"peak": 1}, {"tally": 1}, {"tools": 1}])
add("_emit_session_summary",
    state={"session": sum_sess(turns=0), "_lineages": {}, "_lineage_of": {}},
    io={"eff.ToolUsage": {"tools": 1}, "rt._close_segment": listret([None, None, None]),
        "rt._emit_summary_rows": None})
add("_emit_session_summary",
    state={"session": sum_sess(start_fields={"entrypoint": "cli", "execution_surface": "tui"}),
           "_lineages": {}, "_lineage_of": {}},
    io={"engagement.SessionTally": {"tally": 1}, "model.attended": True,
        "rt._close_segment": MERGED, "monotonic_ns": 100,
        "model_state.quick_abandon_route": None, "rt._emit_summary_rows": None})
add("_emit_session_summary",
    state={"session": sum_sess(start_fields={"entrypoint": "delegated"}),
           "_lineages": {}, "_lineage_of": {}},
    io={"eff.ToolUsage": {"tools": 1}, "rt._close_segment": listret([None, None, None]),
        "rt._emit_summary_rows": None})
add("_emit_session_summary",
    state={"session": sum_sess(), "_lineages": {}, "_lineage_of": {}},
    io={"engagement.SessionTally": {"tally": 1}, "model.attended": seq(True, True),
        "rt._close_segment": MERGED, "monotonic_ns": 100,
        "model_state.quick_abandon_route": ROUTE, "model.friction_fields": {"f": "1"},
        "rt._emit_summary_rows": None})
add("_emit_session_summary",
    state={"session": sum_sess(retiring=True), "_lineages": {}, "_lineage_of": {}},
    io={"engagement.SessionTally": {"tally": 1}, "model.attended": True,
        "rt._close_segment": MERGED, "rt._emit_summary_rows": None})
add("_emit_session_summary",
    state={"session": sum_sess(turns=0), "_lineages": {}, "_lineage_of": {}},
    io={"eff.ToolUsage": None})
add("_emit_session_summary",
    state={"session": sum_sess(), "_lineages": {}, "_lineage_of": {}},
    io={"engagement.SessionTally": raise_("SystemExit")})
add("_emit_session_summary",
    state={"session": sum_sess(turns=0), "_lineages": {}, "_lineage_of": {}}, io={})

# ── record_process_mark ──
add("record_process_mark", mark="hermes.x", data={"a": "1"},
    io={"rt._event_metadata": {"schema": "v"}, "rt._with_scope_stack": None})
add("record_process_mark", mark="hermes.x", data=None,
    io={"rt._event_metadata": {"schema": "v"}, "rt._with_scope_stack": None})
add("record_process_mark", mark="hermes.x", data={"a": "1"},
    io={"rt._event_metadata": None, "rt._with_scope_stack": None})
add("record_process_mark", mark="hermes.x", data={},
    io={"rt._event_metadata": {}, "rt._with_scope_stack": raise_("SystemExit")})
add("record_process_mark", mark="hermes.x", data={},
    io={"rt._event_metadata": raise_("ValueError")})
add("record_process_mark", mark="hermes.x", data={}, io={})

# ── record_process_marks_saved ──
RPM_IO = {"uuid.uuid4": {"hex": "T"}, "rt._event_metadata": {"schema": "v"},
          "rt._with_scope_stack": None, "relay.subscribers.flush": None,
          "subscriber.take_saved": 2}
add("record_process_marks_saved", marks=[["hermes.a", {"x": "1"}], ["hermes.b", {"y": "2"}]],
    state={"subscriber": {}}, io=RPM_IO)
add("record_process_marks_saved", marks=[], state={"subscriber": {}},
    io=dict(RPM_IO, **{"subscriber.take_saved": 0}))
add("record_process_marks_saved", marks=None, state={"subscriber": {}},
    io={"uuid.uuid4": {"hex": "T"}, "rt._event_metadata": {}})
add("record_process_marks_saved", marks=[["only"]], state={"subscriber": {}},
    io={"uuid.uuid4": {"hex": "T"}, "rt._event_metadata": {}})
add("record_process_marks_saved", marks=[["m", {}]], state={"subscriber": {}},
    io={"uuid.uuid4": {"hex": "T"}, "rt._event_metadata": None})
add("record_process_marks_saved", marks=[["m", {}]], state={"subscriber": {}},
    io={"uuid.uuid4": None})
add("record_process_marks_saved", marks=[["m", {}]], state={"subscriber": {}},
    io={"uuid.uuid4": {"hex": "T"}, "rt._event_metadata": {}, "rt._with_scope_stack": None,
        "relay.subscribers.flush": raise_("KeyboardInterrupt")})
add("record_process_marks_saved", marks=[["m", {}]], state={"subscriber": {}}, io={})

# ── still_consented ──
add("still_consented", endpoint="e1", io={"_resolved_send_config": {"send": True, "endpoint": "e1"}})
add("still_consented", endpoint="e1", io={"_resolved_send_config": {"send": True, "endpoint": "e2"}})
add("still_consented", endpoint="e1", io={"_resolved_send_config": {"send": False, "endpoint": "e1"}})
add("still_consented", endpoint="e1", io={"_resolved_send_config": {"send": None, "endpoint": "e1"}})
add("still_consented", endpoint="e1", io={"_resolved_send_config": {"send": 0, "endpoint": "e1"}})
add("still_consented", endpoint="e1", io={"_resolved_send_config": None})
add("still_consented", endpoint="e1", io={"_resolved_send_config": raise_("SystemExit")})
add("still_consented", endpoint="e1", io={})
add("still_consented", endpoint="e1", io={"_resolved_send_config": {"send": True}})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for rec in CASES:
            f.write(json.dumps(rec, ensure_ascii=True, separators=(",", ":")) + "\n")
    counts = {}
    for rec in CASES:
        counts[rec["func"]] = counts.get(rec["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for fn in sorted(counts):
        print("  %-28s %d" % (fn, counts[fn]))
    assert all(v >= 5 for v in counts.values()), "need >=5 cases per func"
    assert len(counts) == 10, "need all 10 funcs"


if __name__ == "__main__":
    main()