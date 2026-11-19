#!/usr/bin/env python3
"""Generate tests/fixtures/relay_metrics_flow.jsonl — the scripted-seam case
corpus for the relay_metrics_flow FLOW port (the UNCLAIMED third-wave remainder
of hermes_cli/observability/relay_shared_metrics.py). >=5 cases per ported func,
covering happy paths, branch gates, scripted Exception (swallowed by guards),
scripted BaseException (passthrough), CM enter/exit/suppress/override paths,
and fail-closed missing scripts.

Each case: {"func": <live_name>, "in": {...args..., "io": {seam: script},
"state": {...mutable carrier...}}}. A bare JSON array script is a per-call
queue; a list RESULT is wrapped {"__script__": true, "ret": [...]}; a raise is
{"__raise__": kind, "message": msg}. Deterministic (no randomness).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "relay_metrics_flow.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def raise_(kind, msg="boom"):
    return {"__raise__": kind, "message": msg}


def seq(*vals):
    return list(vals)


def listret(arr):
    return {"__script__": True, "ret": arr}


SESS = {"session_id": "s1", "relay_session": {"handle": "H"}, "closing": False,
        "tasks": {}, "efficiency": {"spent": []},
        "model_state": {"last_route": {"provider": "p", "model": "m"}},
        "route_run": {"ref": "RR1"}}
TASK = {"task_id": "t1", "handle": "TH", "ref": "T1"}
RUNTIME = {"__runtime__": True, "id": "r1"}
THREAD = {"__thread__": True}
STORE = {"__store__": True}
HOST = {"profile_key": "p1"}
CM_CONN = {"__cm__": "connection"}
CM_WTXN = {"__cm__": "write_txn"}
CONN = {"__conn__": True}
PEAK_IN = {"peak_route": "r", "peak_tokens": 10, "peak_window": 100, "limit_hit": False}
TALLY_IN = {"turns": 2}
TOOLS_IN = {"__tools__": True}


def lineage(open_ids, run=None, connectors=None, spent=None, peak=None, tally=None, tools=None):
    return {"open": sorted(open_ids),
            "peak": PEAK_IN if peak is None else peak,
            "tally": TALLY_IN if tally is None else tally,
            "tools": TOOLS_IN if tools is None else tools,
            "run": {"ref": "RUN"} if run is None else run,
            "spent": ["turn1"] if spent is None else spent,
            "connectors": {"chain": 1} if connectors is None else connectors}


def resolved(send, endpoint="e1"):
    return {"send": send, "endpoint": endpoint}


def reconcile_io(send_enabled=True):
    return {"store._connection": CM_CONN, "connection.__enter__": CONN,
            "write_txn": CM_WTXN, "write_txn.__enter__": None,
            "reconcile_send_consent": None, "write_txn.__exit__": None,
            "connection.__exit__": None}


def owner_io():
    return {"set_hermes_home_override": "TOK", "reset_hermes_home_override": None}


def flush_ok_io():
    d = {"relay.subscribers.flush": None, "rt._export": None}
    d.update(owner_io())
    return d


# ── _run_scoped ──
add("_run_scoped", session=SESS, task=TASK, callback={"__callable__": "cb"},
    args=[1], kwargs={"k": "v"}, io={"rt._run_in_task": "R"})
add("_run_scoped", session=SESS, task=None, callback={"__callable__": "cb"},
    args=[], kwargs={}, io={"host.run_in_session": None})
add("_run_scoped", session=SESS, task=TASK, callback={"__callable__": "cb"},
    io={"rt._run_in_task": raise_("ValueError")})
add("_run_scoped", session=SESS, task=None, callback={"__callable__": "cb"},
    io={"host.run_in_session": raise_("SystemExit")})
add("_run_scoped", session=SESS, task=TASK, callback={"__callable__": "cb"}, io={})
add("_run_scoped", session={"session_id": "s"}, task=None, callback={"__callable__": "cb"},
    args=["x"], kwargs={"handle": "H"}, io={"host.run_in_session": "OK"})

# ── _mark ──
MARK_IO = {"_scope_handle": "H", "rt._event_metadata": {"schema": "v"},
           "host.run_in_session": None}
add("_mark", session=SESS, task=None, name="hermes.x", data={"a": "1"}, io=MARK_IO)
add("_mark", session=SESS, task=TASK, name="hermes.x", data={"a": "1"},
    io={"_scope_handle": "TH", "rt._event_metadata": {}, "rt._run_in_task": None})
add("_mark", session=SESS, task=None, name="hermes.x", data={},
    io={"_scope_handle": raise_("ValueError")})
add("_mark", session=SESS, task=None, name="hermes.x", data={},
    io={"_scope_handle": "H", "rt._event_metadata": raise_("SystemExit")})
add("_mark", session=SESS, task=None, name="hermes.x", data={}, io={})
add("_mark", session=SESS, task=None, name="hermes.x", data=None,
    io={"_scope_handle": None, "rt._event_metadata": None, "host.run_in_session": None})

# ── _emit_install_snapshot_if_due ──
SNAP_CHAIN = {"_scope_handle": "H", "rt._event_metadata": {"schema": "v"},
              "host.run_in_session": None}
add("_emit_install_snapshot_if_due", session=SESS, state={"_snapshot_checked_ns": None},
    io=dict({"monotonic_ns": 100, "store.install_snapshot_due": True,
             "_raw_config": {"a": 1}, "collect_install_snapshot": {"f": "1"}}, **SNAP_CHAIN))
add("_emit_install_snapshot_if_due", session=SESS, state={"_snapshot_checked_ns": 50},
    io={"monotonic_ns": 100})
add("_emit_install_snapshot_if_due", session=SESS, state={"_snapshot_checked_ns": 0},
    io={"monotonic_ns": 3600000000000, "store.install_snapshot_due": False})
add("_emit_install_snapshot_if_due", session=SESS, state={"_snapshot_checked_ns": None},
    io={"monotonic_ns": 100, "store.install_snapshot_due": True, "_raw_config": {},
        "collect_install_snapshot": raise_("ValueError")})
add("_emit_install_snapshot_if_due", session=SESS, state={"_snapshot_checked_ns": None},
    io={"monotonic_ns": raise_("SystemExit")})
add("_emit_install_snapshot_if_due", session=SESS, state={"_snapshot_checked_ns": None}, io={})
add("_emit_install_snapshot_if_due", session=SESS, state={"_snapshot_checked_ns": None},
    io=dict({"monotonic_ns": 7, "store.install_snapshot_due": True, "_raw_config": None,
             "collect_install_snapshot": None}, **SNAP_CHAIN))

# ── _owner_scope ──
add("_owner_scope", state={"host": HOST}, io=owner_io())
add("_owner_scope", state={"host": HOST}, body={"__callable__": "cb"},
    io=dict(owner_io(), cb=None))
add("_owner_scope", state={"host": HOST}, body={"__callable__": "cb"},
    io=dict(owner_io(), cb=raise_("ValueError")))
add("_owner_scope", state={"host": HOST}, body={"__callable__": "cb"},
    io=dict(owner_io(), cb=raise_("SystemExit")))
add("_owner_scope", state={"host": HOST}, body={"__callable__": "cb"},
    io={"set_hermes_home_override": "TOK", "cb": raise_("ValueError"),
        "reset_hermes_home_override": raise_("RuntimeError", "reset")})
add("_owner_scope", state={"host": HOST},
    io={"set_hermes_home_override": raise_("ValueError")})
add("_owner_scope", state={"host": HOST}, io={})
add("_owner_scope", state={}, io={})

# ── _flush_and_export ──
add("_flush_and_export", failure_message="fm", state={"host": HOST}, io=flush_ok_io())
add("_flush_and_export", failure_message="fm", state={"host": HOST},
    io={"relay.subscribers.flush": raise_("ValueError"), "logger.warning": None})
add("_flush_and_export", failure_message="fm", state={"host": HOST},
    io={"relay.subscribers.flush": raise_("SystemExit")})
add("_flush_and_export", failure_message="fm", state={"host": HOST},
    io=dict(flush_ok_io(), **{"rt._export": raise_("ValueError")}))
add("_flush_and_export", failure_message="fm", state={"host": HOST},
    io={"relay.subscribers.flush": raise_("ValueError")})   # fail-closed logger.warning
add("_flush_and_export", failure_message="fm", state={"host": HOST}, io={})

# ── _run_flush_worker ──
FW_ST = {"_flush_pending": True, "_flush_thread": THREAD, "host": HOST}
add("_run_flush_worker", failure_message="fm", state=FW_ST, io=flush_ok_io())
add("_run_flush_worker", failure_message="fm",
    state={"_flush_pending": False, "_flush_thread": THREAD, "host": HOST}, io={})
add("_run_flush_worker", failure_message="fm", state=FW_ST,
    io={"relay.subscribers.flush": raise_("ValueError"), "logger.warning": None})
add("_run_flush_worker", failure_message="fm", state=FW_ST,
    io={"relay.subscribers.flush": raise_("SystemExit")})
add("_run_flush_worker", failure_message="fm", state=FW_ST, io={})
add("_run_flush_worker", failure_message="fm", state=FW_ST,
    io=dict(flush_ok_io(), **{"rt._export": raise_("ValueError")}))

# ── _schedule_flush_and_export ──
add("_schedule_flush_and_export", failure_message="fm",
    state={"_flush_pending": False, "_flush_thread": None},
    io={"spawn_context_thread": THREAD, "thread.start": None})
add("_schedule_flush_and_export", failure_message="fm",
    state={"_flush_pending": False, "_flush_thread": THREAD}, io={})
add("_schedule_flush_and_export", failure_message="fm",
    state={"_flush_pending": False, "_flush_thread": None},
    io={"spawn_context_thread": raise_("ValueError")})
add("_schedule_flush_and_export", failure_message="fm",
    state={"_flush_pending": False, "_flush_thread": None},
    io={"spawn_context_thread": THREAD, "thread.start": raise_("RuntimeError")})
add("_schedule_flush_and_export", failure_message="fm",
    state={"_flush_pending": False, "_flush_thread": None},
    io={"spawn_context_thread": raise_("SystemExit")})
add("_schedule_flush_and_export", failure_message="fm",
    state={"_flush_pending": False, "_flush_thread": None}, io={})

# ── _close_and_export ──
CE_ST = {"_active": True, "_sessions": {"s1": {}, "s2": {}},
         "_lineage_of": {"s3": "root"}, "_registered": True, "id": "r1", "host": HOST}
add("_close_and_export", state=CE_ST,
    io=dict(flush_ok_io(), **{"rt._safe": None, "rt._deregister": None, "rt._release": None}))
add("_close_and_export", state=dict(CE_ST, _registered=False), io={"rt._safe": None})
add("_close_and_export",
    state={"_active": True, "_sessions": {}, "_lineage_of": {}, "_registered": True,
           "id": "r1", "host": HOST},
    io=dict(flush_ok_io(), **{"rt._deregister": None, "rt._release": None}))
add("_close_and_export", state=CE_ST, io={"rt._safe": raise_("ValueError")})
add("_close_and_export", state=CE_ST, io={"rt._safe": seq(None, raise_("SystemExit"))})
add("_close_and_export", state=CE_ST,
    io=dict(flush_ok_io(), **{"rt._safe": None, "rt._deregister": None, "rt._release": None,
                              "relay.subscribers.flush": raise_("ValueError"),
                              "logger.warning": None}))
add("_close_and_export", state=CE_ST, io={})

# ── _share_spent ──
SS_ST = {"session": SESS, "_lineage_of": {"s1": "root"}, "_lineages": {"root": lineage(["s1"])}}
add("_share_spent", state=SS_ST, io={"rt._lineage_spent": listret(["t1"])})
add("_share_spent", state=SS_ST, io={"rt._lineage_spent": None})
add("_share_spent", state={"session": SESS, "_lineage_of": {}, "_lineages": {}},
    io={"rt._lineage_spent": listret(["t2"])})
add("_share_spent",
    state={"session": SESS, "_lineage_of": {"s1": "root"},
           "_lineages": {"root": lineage(["s1"], connectors=None)}},
    io={"rt._lineage_spent": listret([])})
add("_share_spent", state=SS_ST, io={"rt._lineage_spent": raise_("ValueError")})
add("_share_spent", state=SS_ST, io={})

# ── _with_route_run ──
WR_ST = {"_lineage_of": {"s1": "root"}, "_lineages": {"root": lineage(["s1"])}}
add("_with_route_run", session_id="s1", own={"ref": "OWN"}, update={"__callable__": "cb"},
    state=WR_ST, io={"cb": "R"})
add("_with_route_run", session_id="sX", own={"ref": "OWN"}, update={"__callable__": "cb"},
    state=WR_ST, io={"cb": None})
add("_with_route_run", session_id="s1", own={"ref": "OWN"}, update={"__callable__": "cb"},
    state={"_lineage_of": {"s1": "root"}, "_lineages": {"root": lineage(["s1"], run=None)}},
    io={})
add("_with_route_run", session_id="sX", own=None, update={"__callable__": "cb"},
    state=WR_ST, io={})
add("_with_route_run", session_id="s1", own=None, update={"__callable__": "cb"},
    state=WR_ST, io={"cb": raise_("ValueError")})
add("_with_route_run", session_id="s1", own=None, update={"__callable__": "cb"},
    state=WR_ST, io={"cb": raise_("SystemExit")})
add("_with_route_run", session_id="s1", own=None, update={"__callable__": "cb"},
    state=WR_ST, io={})

# ── _close_segment ──
CS_ST = {"_lineage_of": {"s1": "root", "s2": "root"},
         "_lineages": {"root": lineage(["s1", "s2"])}}
add("_close_segment", session_id="s1", segment_state=PEAK_IN, tally=TALLY_IN, tools=TOOLS_IN,
    state={"_lineage_of": {"s1": "root"}, "_lineages": {"root": lineage(["s1"])}},
    io={"_absorb_peak": None, "tally.absorb": None, "tools.absorb": None})
add("_close_segment", session_id="s1", segment_state=PEAK_IN, tally=TALLY_IN, tools=TOOLS_IN,
    state=CS_ST, io={"_absorb_peak": None, "tally.absorb": None, "tools.absorb": None})
add("_close_segment", session_id="sX", segment_state=PEAK_IN, tally=TALLY_IN, tools=TOOLS_IN,
    state={"_lineage_of": {}, "_lineages": {}}, io={})
add("_close_segment", session_id="s1", segment_state=None, tally=None, tools=TOOLS_IN,
    state={"_lineage_of": {"s1": "root"}, "_lineages": {"root": lineage(["s1"])}},
    io={"tools.absorb": None})
add("_close_segment", session_id="s1", segment_state=PEAK_IN, tally=TALLY_IN, tools=TOOLS_IN,
    state={"_lineage_of": {"s1": "root"}, "_lineages": {"root": lineage(["s1"])}},
    io={"_absorb_peak": raise_("ValueError")})
add("_close_segment", session_id="s1", segment_state=PEAK_IN, tally=TALLY_IN, tools=TOOLS_IN,
    state={"_lineage_of": {"s1": "root"}, "_lineages": {"root": lineage(["s1"])}},
    io={"_absorb_peak": None, "tally.absorb": None, "tools.absorb": raise_("SystemExit")})
add("_close_segment", session_id="s1", segment_state=PEAK_IN, tally=TALLY_IN, tools=TOOLS_IN,
    state={"_lineage_of": {"s1": "root"}, "_lineages": {"root": lineage(["s1"])}}, io={})

# ── _emit_summary_rows ──
ES_IO = {"peak.context_peak_fields": {"p": "1"}, "tally.fields": {"t": "1"},
         "tools.rows": listret([["hermes.tool", {"x": "1"}]]), "rt._emit_rows": None}
add("_emit_summary_rows", session=None, peak=PEAK_IN, tally=TALLY_IN, tools=TOOLS_IN, io=ES_IO)
add("_emit_summary_rows", session=None, peak=None, tally=TALLY_IN, tools=TOOLS_IN,
    io={k: v for k, v in ES_IO.items() if k != "peak.context_peak_fields"})
add("_emit_summary_rows", session=None, peak=PEAK_IN, tally=None, tools=None,
    io={"peak.context_peak_fields": {"p": "1"}, "rt._emit_rows": None})
add("_emit_summary_rows", session=None, peak=None, tally=None, tools=None,
    extra=[["hermes.extra", {"e": "1"}]], io={"rt._emit_rows": None})
add("_emit_summary_rows", session=None, peak=PEAK_IN, tally=None, tools=None,
    io={"peak.context_peak_fields": raise_("ValueError")})
add("_emit_summary_rows", session=None, peak=None, tally=None, tools=None,
    io={"rt._emit_rows": raise_("SystemExit")})
add("_emit_summary_rows", session=None, peak=PEAK_IN, tally=None, tools=None, io={})
add("_emit_summary_rows", session=SESS, peak=None, tally=None, tools=None,
    io={"rt._emit_rows": None})

# ── _close_unseen_segment ──
CU_ST = {"_lineage_of": {"s1": "root"}, "_lineages": {"root": lineage(["s1"])}}
CU_IO = {"eff.ToolUsage": TOOLS_IN, "tools.absorb": None,
         "peak.context_peak_fields": {"p": "1"}, "tally.fields": {"t": "1"},
         "tools.rows": listret([]), "rt._emit_rows": None}
add("_close_unseen_segment", session_id="s1", state=CU_ST, io=CU_IO)
add("_close_unseen_segment", session_id="", state=CU_ST, io={})
add("_close_unseen_segment", session_id="sX", state={"_lineage_of": {}, "_lineages": {}},
    io={"eff.ToolUsage": TOOLS_IN, "tools.rows": listret([]), "rt._emit_rows": None})
add("_close_unseen_segment", session_id="s1", state=CU_ST,
    io={"eff.ToolUsage": raise_("ValueError")})
add("_close_unseen_segment", session_id="s1", state=CU_ST,
    io={"eff.ToolUsage": TOOLS_IN, "tools.absorb": raise_("SystemExit")})
add("_close_unseen_segment", session_id="s1", state=CU_ST, io={})

# ── _flush_lineage ──
FL_ST = {"_lineage_of": {"s1": "root", "s2": "root"},
         "_lineages": {"root": lineage(["s1", "s2"])}}
FL_IO = {"peak.context_peak_fields": {"p": "1"}, "tally.fields": {"t": "1"},
         "tools.rows": listret([["hermes.tool", {"x": "1"}]]), "rt._emit_rows": None}
add("_flush_lineage", key="root", state=FL_ST, io=FL_IO)
add("_flush_lineage", key="absent", state=FL_ST, io={})
add("_flush_lineage", key="root",
    state={"_lineage_of": {}, "_lineages": {"root": lineage([])}}, io=FL_IO)
add("_flush_lineage", key="root",
    state={"_lineage_of": {"s1": "root"},
           "_lineages": {"root": lineage(["s1"], peak=None, tally=None)}},
    io={"tools.rows": listret([]), "rt._emit_rows": None})
add("_flush_lineage", key="root", state=FL_ST,
    io=dict(FL_IO, **{"rt._emit_rows": raise_("ValueError")}))
add("_flush_lineage", key="root", state=FL_ST, io={})

# ── _send_exported_packages ──
SEP_ST = {"_send_thread": None, "subscriber": {"store": STORE}}
add("_send_exported_packages", state=SEP_ST,
    io={"_resolved_send_config": resolved(True), "rt._guarded": None,
        "spawn_context_thread": THREAD, "thread.start": None})
add("_send_exported_packages", state=SEP_ST,
    io={"_resolved_send_config": resolved(False), "rt._guarded": None})
add("_send_exported_packages",
    state={"_send_thread": THREAD, "subscriber": {"store": STORE}},
    io={"_resolved_send_config": resolved(True), "rt._guarded": None, "thread.is_alive": True})
add("_send_exported_packages",
    state={"_send_thread": THREAD, "subscriber": {"store": STORE}},
    io={"_resolved_send_config": resolved(True), "rt._guarded": None,
        "thread.is_alive": False, "spawn_context_thread": THREAD, "thread.start": None})
add("_send_exported_packages", state=SEP_ST,
    io={"_resolved_send_config": raise_("ValueError"), "logger.debug": None})
add("_send_exported_packages", state=SEP_ST,
    io={"_resolved_send_config": raise_("SystemExit")})
add("_send_exported_packages", state=SEP_ST, io={"_resolved_send_config": None})
add("_send_exported_packages", state=SEP_ST,
    io={"_resolved_send_config": resolved(True), "rt._guarded": raise_("RuntimeError")})
add("_send_exported_packages", state=SEP_ST, io={})
add("_send_exported_packages", state=SEP_ST,
    io={"_resolved_send_config": resolved(True), "rt._guarded": None,
        "spawn_context_thread": raise_("ValueError")})
add("_send_exported_packages", state=SEP_ST,
    io={"_resolved_send_config": resolved(True), "rt._guarded": None,
        "spawn_context_thread": THREAD, "thread.start": raise_("RuntimeError")})

# ── _run_send_pass ──
RSP_ST = {"subscriber": {"store": STORE}}
add("_run_send_pass", endpoint="e1", state=RSP_ST,
    io={"SharedMetricsSender": {"__sender__": True}, "rt._guarded": None})
add("_run_send_pass", endpoint="e1", state=RSP_ST,
    io={"SharedMetricsSender": raise_("ValueError")})
add("_run_send_pass", endpoint="e1", state=RSP_ST, io={"SharedMetricsSender": None})
add("_run_send_pass", endpoint="e1", state=RSP_ST,
    io={"SharedMetricsSender": {"__sender__": True}, "rt._guarded": raise_("SystemExit")})
add("_run_send_pass", endpoint="e1", state=RSP_ST,
    io={"SharedMetricsSender": {"__sender__": True}, "rt._guarded": raise_("ValueError")})
add("_run_send_pass", endpoint="e1", state=RSP_ST, io={})

# ── record_auxiliary_tokens ──
add("record_auxiliary_tokens",
    event={"usage": {"input": 5}, "aux_task": "title", "response_model": "rm",
           "model": "m", "provider": "p"},
    io={"fields.model_token_fields": {"tokens": "5"}, "rt._event_metadata": {"schema": "v"},
        "rt._with_scope_stack": None})
add("record_auxiliary_tokens", event={"usage": None},
    io={"fields.model_token_fields": None})
add("record_auxiliary_tokens", event={"model": "m", "provider": "p"},
    io={"fields.model_token_fields": {"tokens": "1"}, "rt._event_metadata": None,
        "rt._with_scope_stack": None})
add("record_auxiliary_tokens", event={},
    io={"fields.model_token_fields": raise_("ValueError")})
add("record_auxiliary_tokens", event={"response_model": "rm"},
    io={"fields.model_token_fields": {"tokens": "1"}, "rt._event_metadata": {},
        "rt._with_scope_stack": raise_("SystemExit")})
add("record_auxiliary_tokens", event={}, io={})

# ── record_switch_after ──
RSA_ST = {"_lineage_of": {"s1": "root"}, "_lineages": {"root": lineage(["s1"])}}
add("record_switch_after", session_id="s1", state=RSA_ST,
    io={"rt._session": SESS, "run.take": listret(["m", 2]),
        "engagement.switch_after_fields": {"switched": "1"}, "rt.record_process_mark": None})
add("record_switch_after", session_id="s1", state=RSA_ST,
    io={"rt._session": SESS, "run.take": None})
add("record_switch_after", session_id="s1", state=RSA_ST, io={"rt._session": None})
add("record_switch_after", session_id="s1", state=RSA_ST,
    io={"rt._session": None, "run.take": listret(["m", 1]),
        "engagement.switch_after_fields": {"switched": "1"}, "rt.record_process_mark": None})
add("record_switch_after", session_id="s1", state=RSA_ST,
    io={"rt._session": SESS, "run.take": raise_("ValueError")})
add("record_switch_after", session_id="s1", state=RSA_ST,
    io={"rt._session": SESS, "run.take": listret(["m", 2]),
        "engagement.switch_after_fields": raise_("SystemExit")})
add("record_switch_after", session_id="s1", state=RSA_ST, io={"rt._session": SESS})

# ── record_friction ──
RF_IO = {"rt._session": SESS,
         "efficiency.discard_turns": listret([["hermes.waste", {"w": "1"}]]),
         "model.friction_fields": {"f": "1"}, "rt.record_process_mark": None,
         "rt._emit_rows": None}
add("record_friction", signal="undo", session_id="s1", fallback_route={"provider": "fb"},
    turns=2, io=RF_IO)
add("record_friction", signal="other", session_id="s1", fallback_route={"provider": "fb"},
    io={"rt._session": SESS, "model.friction_fields": {"f": "1"},
        "rt.record_process_mark": None, "rt._emit_rows": None})
add("record_friction", signal="retry", session_id="s1", fallback_route={},
    io={"rt._session": None, "eff.SessionEfficiency": {"__eff__": True},
        "rt._lineage_spent": listret(["t1"]),
        "efficiency.discard_turns": listret([]), "model.friction_fields": {"f": "1"},
        "rt.record_process_mark": None, "rt._emit_rows": None})
add("record_friction", signal="interrupt", session_id="s1", fallback_route={},
    io={"rt._session": None, "eff.SessionEfficiency": {"__eff__": True},
        "rt._lineage_spent": None, "efficiency.discard_turns": listret([]),
        "model.friction_fields": {"f": "1"}, "rt.record_process_mark": None,
        "rt._emit_rows": None})
add("record_friction", signal="other", session_id="", fallback_route={"provider": "fb"},
    io={"model.friction_fields": {"f": "1"}, "rt.record_process_mark": None,
        "rt._emit_rows": None})
add("record_friction", signal="undo", session_id="s1", fallback_route={},
    io={"rt._session": SESS, "efficiency.discard_turns": listret([]),
        "model.friction_fields": None})
add("record_friction", signal="undo", session_id="s1", fallback_route={},
    io={"rt._session": SESS, "efficiency.discard_turns": raise_("ValueError")})
add("record_friction", signal="other", session_id="s1", fallback_route={},
    io={"rt._session": SESS, "model.friction_fields": raise_("SystemExit")})
add("record_friction", signal="undo", session_id="s1", fallback_route={},
    io={"rt._session": SESS})

# ── _reconcile_store_consent ──
add("_reconcile_store_consent", store=STORE, send_enabled=True, io=reconcile_io())
add("_reconcile_store_consent", store=STORE, send_enabled=False,
    io=dict(reconcile_io(), **{"reconcile_send_consent": raise_("ValueError")}))
add("_reconcile_store_consent", store=STORE, send_enabled=True,
    io=dict(reconcile_io(), **{"reconcile_send_consent": raise_("ValueError"),
                               "write_txn.__exit__": True}))
add("_reconcile_store_consent", store=STORE, send_enabled=True,
    io=dict(reconcile_io(), **{"reconcile_send_consent": raise_("ValueError"),
                               "write_txn.__exit__": raise_("RuntimeError", "exit")}))
add("_reconcile_store_consent", store=STORE, send_enabled=True,
    io={"store._connection": CM_CONN, "connection.__enter__": raise_("ValueError")})
add("_reconcile_store_consent", store=STORE, send_enabled=True,
    io={"store._connection": CM_CONN, "connection.__enter__": CONN,
        "write_txn": raise_("ValueError"), "connection.__exit__": None})
add("_reconcile_store_consent", store=STORE, send_enabled=True,
    io={"store._connection": None})
add("_reconcile_store_consent", store=STORE, send_enabled=True,
    io=dict(reconcile_io(), **{"connection.__exit__": raise_("RuntimeError", "outer")}))
add("_reconcile_store_consent", store=STORE, send_enabled=True, io={})
add("_reconcile_store_consent", store=STORE, send_enabled=True,
    io=dict(reconcile_io(), **{"reconcile_send_consent": raise_("SystemExit")}))

# ── _reconcile_send_consent_once ──
RSC_IO = dict(reconcile_io(), **{"_resolved_send_config": resolved(True),
                                 "get_hermes_home": "/h/p1", "SharedMetricsStore": STORE})
add("_reconcile_send_consent_once", state={"_consent_reconcile_done": True}, io={})
add("_reconcile_send_consent_once", state={"_consent_reconcile_done": False}, io=RSC_IO)
add("_reconcile_send_consent_once", state={"_consent_reconcile_done": False},
    io={"_resolved_send_config": resolved(False), "get_hermes_home": "/h/p1",
        "path.exists": False})
add("_reconcile_send_consent_once", state={"_consent_reconcile_done": False},
    io=dict(RSC_IO, **{"_resolved_send_config": resolved(False), "path.exists": True}))
add("_reconcile_send_consent_once", state={"_consent_reconcile_done": False},
    io={"_resolved_send_config": raise_("ValueError"), "logger.warning": None})
add("_reconcile_send_consent_once", state={"_consent_reconcile_done": False},
    io={"_resolved_send_config": raise_("SystemExit")})
add("_reconcile_send_consent_once", state={"_consent_reconcile_done": False},
    io={"_resolved_send_config": resolved(True), "get_hermes_home": None,
        "logger.warning": None})
add("_reconcile_send_consent_once", state={"_consent_reconcile_done": False},
    io=dict(RSC_IO, **{"reconcile_send_consent": raise_("ValueError"),
                       "logger.warning": None}))
add("_reconcile_send_consent_once", state={"_consent_reconcile_done": False}, io={})

# ── record_session_friction (module) ──
add("record_session_friction", signal="undo", session_id="s1",
    fallback_route={"provider": "fb"}, turns=2,
    io={"relay_runtime.relay_instrumentation_enabled": True, "_get_runtime": RUNTIME,
        "rt._safe": None})
add("record_session_friction", signal="undo", session_id="s1", fallback_route={},
    io={"relay_runtime.relay_instrumentation_enabled": False})
add("record_session_friction", signal="undo", session_id="s1", fallback_route={},
    io={"relay_runtime.relay_instrumentation_enabled": True, "_get_runtime": None})
add("record_session_friction", signal="undo", session_id="s1", fallback_route={},
    io={"relay_runtime.relay_instrumentation_enabled": True, "_get_runtime": RUNTIME,
        "rt._safe": raise_("ValueError")})
add("record_session_friction", signal="undo", session_id="s1", fallback_route={},
    io={"relay_runtime.relay_instrumentation_enabled": raise_("SystemExit")})
add("record_session_friction", signal="undo", session_id="s1", fallback_route={}, io={})

# ── record_session_tools (module) ──
RST_ST = {"_RUNTIMES": {"p1": RUNTIME}}
add("record_session_tools", session_id="s1", agent={"provider": "p", "model": "m"},
    tools_for_api=[{"name": "t1"}], state=RST_ST,
    io={"relay_runtime.current_profile_key": "p1", "rt._safe": None})
add("record_session_tools", session_id="s1", agent=None, tools_for_api=[],
    state={"_RUNTIMES": {}}, io={"relay_runtime.current_profile_key": "p1"})
add("record_session_tools", session_id="s1", agent=None, tools_for_api=[],
    state={"_RUNTIMES": {"p1": {"__failed__": True}}},
    io={"relay_runtime.current_profile_key": "p1"})
add("record_session_tools", session_id="s1", agent=None, tools_for_api=[], state=RST_ST,
    io={"relay_runtime.current_profile_key": "p1", "rt._safe": raise_("ValueError")})
add("record_session_tools", session_id="s1", agent=None, tools_for_api=[], state=RST_ST,
    io={"relay_runtime.current_profile_key": raise_("SystemExit")})
add("record_session_tools", session_id="s1", agent=None, tools_for_api=[], state=RST_ST, io={})

# ── record_known_cache_break (module) ──
add("record_known_cache_break", cause="compaction", route={"provider": "p"}, session_id="s1",
    io={"relay_runtime.relay_instrumentation_enabled": True, "_get_runtime": RUNTIME,
        "rt._safe": None})
add("record_known_cache_break", cause="compaction", route={}, session_id="s1",
    io={"relay_runtime.relay_instrumentation_enabled": False})
add("record_known_cache_break", cause="compaction", route={}, session_id="s1",
    io={"relay_runtime.relay_instrumentation_enabled": True, "_get_runtime": None})
add("record_known_cache_break", cause="compaction", route={}, session_id="s1",
    io={"relay_runtime.relay_instrumentation_enabled": True, "_get_runtime": RUNTIME,
        "rt._safe": raise_("ValueError")})
add("record_known_cache_break", cause="compaction", route={}, session_id="s1",
    io={"relay_runtime.relay_instrumentation_enabled": raise_("KeyboardInterrupt")})
add("record_known_cache_break", cause="compaction", route={}, session_id="s1", io={})

# ── record_model_switch_after (module) ──
add("record_model_switch_after", session_id="s1", state=RST_ST,
    io={"enabled": True, "relay_runtime.relay_instrumentation_enabled": True,
        "relay_runtime.current_profile_key": "p1", "rt._safe": None})
add("record_model_switch_after", session_id="", io={})
add("record_model_switch_after", session_id="s1", io={"enabled": False})
add("record_model_switch_after", session_id="s1",
    io={"enabled": True, "relay_runtime.relay_instrumentation_enabled": False})
add("record_model_switch_after", session_id="s1", state={"_RUNTIMES": {}},
    io={"enabled": True, "relay_runtime.relay_instrumentation_enabled": True,
        "relay_runtime.current_profile_key": "p1"})
add("record_model_switch_after", session_id="s1",
    io={"enabled": raise_("SystemExit")})
add("record_model_switch_after", session_id="s1", io={})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(counts), OUT))
    if short:
        print("WARNING funcs with <5 cases: %s" % short)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())