#!/usr/bin/env python3
"""Generate tests/fixtures/hosted_room_driver_flow.jsonl for the
tui_gateway/hosted_room_driver.py FLOW port (hosted_room_driver_flow).

Deterministic, hand-picked branch/edge/raise/fail-closed cases: >=5 per
ported function. Shapes match the harness contract:
  {"func": <py name>, "in": {"state": {...}, "io": {...}, ...args}}
io scripts: bare value = constant; bare array = per-call queue;
{"__script__": true, "ret": v} = constant list/object; a missing/exhausted
script is fail-closed (RuntimeError spec) on BOTH sides.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "hosted_room_driver_flow.jsonl")

IDENT = {"room_id": "r1", "task_id": "t1", "thread_id": "th1", "turn_id": "tu1"}


def ident(**kw):
    return {**IDENT, **kw}


def binding(room_id="r1", gateway_id="g1", authority_epoch=2):
    return {"room_id": room_id, "gateway_id": gateway_id, "authority_epoch": authority_epoch}


def lease(room_id="r1", gateway_id="g1", authority_epoch=2, process_generation="pg-test",
          lease_generation=1, expires_at=1000.0, reclaimed=False):
    return {"room_id": room_id, "gateway_id": gateway_id, "authority_epoch": authority_epoch,
            "process_generation": process_generation, "lease_generation": lease_generation,
            "expires_at": expires_at, "reclaimed": reclaimed}


def attempt(room_id="r1", task_id="t1", execution_generation=3, cancel_generation=1,
            lease_gen=1, expires_at=1000.0):
    return {"identity": ident(room_id=room_id, task_id=task_id),
            "lease": lease(room_id=room_id, lease_generation=lease_gen, expires_at=expires_at),
            "execution_generation": execution_generation, "cancel_generation": cancel_generation}


def task(**kw):
    base = {"identity": ident(), "status": "running", "execution_generation": 3,
            "cancel_generation": 1, "cancel_id": "c1", "updated_at": 100.0,
            "payload": {"target_profile": "p1", "target_member_id": "m1", "prompt": "hi"}}
    base.update(kw)
    return base


def state(**kw):
    return kw


def sc(ret):
    return {"__script__": True, "ret": ret}


def rs(kind, msg=""):
    return {"__raise__": kind, "message": msg}


CASES = []


def add(func, io=None, **args):
    CASES.append({"func": func, "in": {"io": io or {}, **args}})


def addst(func, st, io=None, **args):
    CASES.append({"func": func, "in": {"state": st, "io": io or {}, **args}})


# ── _session_kw ──────────────────────────────────────────────────────────────
add("_session_kw", profile="p1", session_id="s1")
add("_session_kw", profile="", session_id="")
add("_session_kw", profile=None, session_id="s1")
add("_session_kw", profile="p\u00e9", session_id="s\u00e9")
add("_session_kw", profile="p1")

# ── _fences ──────────────────────────────────────────────────────────────────
add("_fences", task=task(execution_generation=3, cancel_generation=1))
add("_fences", task=task(execution_generation=3.9, cancel_generation=-2.9))
add("_fences", task=task(execution_generation="5", cancel_generation=" 7 "))
add("_fences", task=task(execution_generation=True, cancel_generation=False))
add("_fences", task=task(execution_generation="abc", cancel_generation=1))
add("_fences", task=task(execution_generation=None, cancel_generation=1))
add("_fences", task={"identity": ident(), "cancel_generation": 1})
add("_fences", task=task(execution_generation="1_2", cancel_generation="+007"))

# ── room_session_title ───────────────────────────────────────────────────────
add("room_session_title", room_id="r1")
add("room_session_title", room_id="")
add("room_session_title", room_id="r\u00e9")
add("room_session_title", room_id=5)
add("room_session_title", room_id=None)

# ── _member_id ───────────────────────────────────────────────────────────────
add("_member_id", task=task())
add("_member_id", task=task(payload={"target_profile": "p1"}))
add("_member_id", task=task(payload={"target_member_id": "", "target_profile": ""}))
add("_member_id", task=task(payload=None))
add("_member_id", task={"identity": ident()})
add("_member_id", task=task(payload=5))
add("_member_id", task=task(payload={"target_member_id": 5}))
add("_member_id", task=task(payload={"target_member_id": True}))
add("_member_id", task=None)

# ── _session_id ──────────────────────────────────────────────────────────────
add("_session_id", session={"session_id": "s1"})
add("_session_id", session={"id": "s2"})
add("_session_id", session={"session_id": None, "id": "s2"})
add("_session_id", session={})
add("_session_id", session={"session_id": 5})
add("_session_id", session={"session_id": ""})
add("_session_id", session=None)
add("_session_id", session=["s1"])

# ── _truncate_utf8 ───────────────────────────────────────────────────────────
add("_truncate_utf8", value="short", max_bytes=65536)
add("_truncate_utf8", value="x" * 65536, max_bytes=65536)
add("_truncate_utf8", value="x" * 70000, max_bytes=65536)
add("_truncate_utf8", value="\u00e9" * 40000, max_bytes=65536)   # cut mid-char backoff
add("_truncate_utf8", value="hello", max_bytes=10)               # < notice len
add("_truncate_utf8", value="hello", max_bytes=0)
add("_truncate_utf8", value="hello", max_bytes=-5)
add("_truncate_utf8", value=None, max_bytes=65536)
add("_truncate_utf8", value=42, max_bytes=65536)
add("_truncate_utf8", value="a\u00e9\u4e2d" * 30000, max_bytes=65536)

# ── _bounded_terminal_result ─────────────────────────────────────────────────
add("_bounded_terminal_result", receipt={"text": "hello", "message_id": "m1"})
add("_bounded_terminal_result", receipt={"text": "ok", "error": "bad"})
add("_bounded_terminal_result", receipt={"text": "x" * 70000, "error": "e" * 5000})
add("_bounded_terminal_result", receipt={"text": "ok", "error": ""})
add("_bounded_terminal_result", receipt={})
add("_bounded_terminal_result", receipt=None)
add("_bounded_terminal_result", receipt={"text": None, "error": None, "message_id": None})

# ── _receipt_from ────────────────────────────────────────────────────────────
add("_receipt_from", receipt={"status": "settled", "settlement_id": "s1", "text": "ok"},
    identity=ident(), execution_generation=3)
add("_receipt_from", receipt={"status": "failed", "text": "ok"}, identity=ident(), execution_generation=3)
add("_receipt_from", receipt={"status": "settled", "settlement_id": ""}, identity=ident(), execution_generation=4)
add("_receipt_from", receipt={"status": "settled"}, identity=None, execution_generation=3)
add("_receipt_from", receipt={"status": "settled", "settlement_id": "keep"}, identity=None, execution_generation=3)
add("_receipt_from", receipt={"status": "other", "text": "x" * 70000}, identity=ident(), execution_generation=3)

# ── discarded_receipt ────────────────────────────────────────────────────────
add("discarded_receipt", result={"status": "settled", "text": "ok"}, task=task())
add("discarded_receipt", result={"status": "failed", "text": "ok"}, task=task())
add("discarded_receipt", result={"status": "cancelled"}, task=task())
add("discarded_receipt", result={}, task=task())
add("discarded_receipt", result={"status": "settled"}, task={"identity": ident()})
add("discarded_receipt", result={"status": "settled"}, task=task(execution_generation="4"))
add("discarded_receipt", result=None, task=task())

# ── _find_terminal_receipt ───────────────────────────────────────────────────
HIST = [{"task_id": "t1", "execution_generation": 3, "role": "assistant", "status": "settled",
         "message_id": "m1", "content": "first"},
        {"task_id": "t1", "execution_generation": 3, "role": "assistant", "status": "failed",
         "message_id": "m2", "content": "second"}]
add("_find_terminal_receipt", history=HIST, identity=ident(), execution_generation=3)
add("_find_terminal_receipt", history=[HIST[0]], identity=ident(), execution_generation=3)
add("_find_terminal_receipt", history=[{"task_id": "other", "execution_generation": 3, "role": "assistant", "status": "settled"}], identity=ident(), execution_generation=3)
add("_find_terminal_receipt", history=[{"task_id": "t1", "execution_generation": 9, "role": "assistant", "status": "settled"}], identity=ident(), execution_generation=3)
add("_find_terminal_receipt", history=[{"task_id": "t1", "execution_generation": 3, "role": "user", "status": "settled"}], identity=ident(), execution_generation=3)
add("_find_terminal_receipt", history=[{"task_id": "t1", "execution_generation": 3, "role": "assistant", "status": "running"}], identity=ident(), execution_generation=3)
add("_find_terminal_receipt", history=[{"task_id": "t1", "execution_generation": 3, "role": "assistant", "status": "settled", "content": "no mid"}], identity=ident(), execution_generation=3)
add("_find_terminal_receipt", history=[], identity=ident(), execution_generation=3)
add("_find_terminal_receipt", history=None, identity=ident(), execution_generation=3)
add("_find_terminal_receipt", history=["x"], identity=ident(), execution_generation=3)
add("_find_terminal_receipt", history=[{"task_id": "t1", "execution_generation": 3.0, "role": "assistant", "status": "failed", "message_id": ""}], identity=ident(), execution_generation=3)

# ── _info_active ─────────────────────────────────────────────────────────────
add("_info_active", info={"active": True})
add("_info_active", info={"active": False, "running": True})
add("_info_active", info={})
add("_info_active", info={"active": None, "running": "x"})
add("_info_active", info=None)
add("_info_active", info={"active": 0, "running": 0})

# ── _info_is_active_for ──────────────────────────────────────────────────────
add("_info_is_active_for", info={"active": True, "task_id": "t1"}, identity=ident())
add("_info_is_active_for", info={"active": True, "task_id": None}, identity=ident(), require_exact=True)
add("_info_is_active_for", info={"active": True, "task_id": None}, identity=ident())
add("_info_is_active_for", info={"active": False, "task_id": "t1"}, identity=ident())
add("_info_is_active_for", info={"active": True, "task_id": "t1"}, identity=None)
add("_info_is_active_for", info={"active": True, "task_id": "other"}, identity=ident())
add("_info_is_active_for", info=None, identity=ident())
# ── _remote ──────────────────────────────────────────────────────────────────
addst("_remote", state(cb={"remote_member": False}), task=task())
addst("_remote", state(cb={"remote_member": True}), io={"remote_member": True}, task=task())
addst("_remote", state(cb={"remote_member": True}), io={"remote_member": False}, task=task())
addst("_remote", state(cb={"remote_member": True}), io={"remote_member": 1}, task=task())
addst("_remote", state(cb={"remote_member": True}), task={"status": "running"})
addst("_remote", state(cb={"remote_member": True}), io={"remote_member": rs("RuntimeError", "boom")}, task=task())
addst("_remote", state(cb={"remote_member": True}), io={"remote_member": rs("SystemExit", "bye")}, task=task())

# ── _publish ─────────────────────────────────────────────────────────────────
addst("_publish", state(cb={"publish_terminal": False}), binding=binding(), task=task())
addst("_publish", state(cb={"publish_terminal": True}), io={"publish_terminal": None}, binding=binding(), task=task())
addst("_publish", state(cb={"publish_terminal": True}), io={"publish_terminal": rs("RuntimeError", "x")}, binding=binding(), task=task())
addst("_publish", state(cb={"publish_terminal": True}), io={"publish_terminal": None}, binding=None, task=task())
addst("_publish", state(cb={"publish_terminal": True}), io={"publish_terminal": rs("KeyboardInterrupt", "")}, binding=binding(), task=task())

# ── _set_blocked ─────────────────────────────────────────────────────────────
addst("_set_blocked", state(blocked=["r0"]), room_id="r1", blocked=True)
addst("_set_blocked", state(blocked=["r1"]), room_id="r1", blocked=True)
addst("_set_blocked", state(blocked=["r1", "r2"]), room_id="r1", blocked=False)
addst("_set_blocked", state(blocked=[]), room_id="r1", blocked=False)
addst("_set_blocked", state(blocked=["r2", "r0"]), room_id="r1", blocked=True)

# ── _record_error ────────────────────────────────────────────────────────────
addst("_record_error", state(last_error="old"), message="new")
addst("_record_error", state(), message="")
addst("_record_error", state(), message="err \u00e9")
addst("_record_error", state(last_error=None), message="x")
addst("_record_error", state(), message="a b c")

# ── _record_task_error ───────────────────────────────────────────────────────
addst("_record_task_error", state(), attempt=attempt(), message="failed: x")
addst("_record_task_error", state(), attempt=None, message="m")
addst("_record_task_error", state(), attempt={"identity": None, "lease": lease(), "execution_generation": 3, "cancel_generation": 1}, message="m")
addst("_record_task_error", state(last_error="old"), attempt=attempt(task_id="t9"), message="fenced: y")
addst("_record_task_error", state(), attempt=attempt(), message="")

# ── _binding_for_room ────────────────────────────────────────────────────────
addst("_binding_for_room", state(rooms=[binding("r1"), binding("r2")]), room_id="r1")
addst("_binding_for_room", state(rooms=[binding("r1"), binding("r2")]), room_id="r2")
addst("_binding_for_room", state(rooms=[binding("r1")]), room_id="r9")
addst("_binding_for_room", state(rooms=[]), room_id="r1")
addst("_binding_for_room", state(rooms=[binding("r1"), binding("r1", gateway_id="g2")]), room_id="r1")
addst("_binding_for_room", state(rooms=[None]), room_id="r1")

# ── _transport_for ───────────────────────────────────────────────────────────
addst("_transport_for", state(cb={"transport_resolver": True}), io={"transport_resolver": {"id": "t1"}}, binding=binding(), task=task())
addst("_transport_for", state(cb={"transport_resolver": True}), io={"transport_resolver": None}, binding=binding(), task=task())
addst("_transport_for", state(cb={"transport_resolver": True}), io={"transport_resolver": rs("RuntimeError", "no route")}, binding=binding(), task=task())
addst("_transport_for", state(cb={"transport_resolver": False, "rpc": True}), binding=binding(), task=task())
addst("_transport_for", state(cb={"transport_resolver": False, "rpc": False}), binding=binding(), task=task())
addst("_transport_for", state(cb={"transport_resolver": True}), io={"transport_resolver": rs("GeneratorExit", "")}, binding=binding(), task=task())

# ── _tasks ───────────────────────────────────────────────────────────────────
addst("_tasks", state(), io={"state.list_tasks": sc([task()])}, binding=binding(), status="queued")
addst("_tasks", state(), io={"state.list_tasks": sc([])}, binding=binding(), status="stopping")
addst("_tasks", state(), io={"state.list_tasks": rs("DriverStateError", "db")}, binding=binding(), status="running")
addst("_tasks", state(), io={"state.list_tasks": sc([])}, binding=binding(), status=None)
addst("_tasks", state(), binding=None, status="queued")

# ── _defer_unavailable_route ─────────────────────────────────────────────────
addst("_defer_unavailable_route", state(), io={"clock": 100.0}, task=task())
addst("_defer_unavailable_route", state(route_retries=[{"room_id": "r1", "member_id": "m1", "delay": 1.0, "next_attempt_at": 50.0}]), io={"clock": 100.0}, task=task())
addst("_defer_unavailable_route", state(route_retries=[{"room_id": "r1", "member_id": "m1", "delay": 20.0, "next_attempt_at": 50.0}]), io={"clock": 100.0}, task=task())
addst("_defer_unavailable_route", state(route_retries=[{"room_id": "r1", "member_id": "m1", "delay": 0.2, "next_attempt_at": 50.0}]), io={"clock": 100.0}, task=task())
addst("_defer_unavailable_route", state(), io={"clock": rs("RuntimeError", "clk")}, task=task())
addst("_defer_unavailable_route", state(), task={"status": "running"})
addst("_defer_unavailable_route", state(), io={"clock": 100.0}, task=task(payload={"target_profile": "p1"}))

# ── _past_execution_deadline ─────────────────────────────────────────────────
addst("_past_execution_deadline", state(), task={"identity": ident()})
addst("_past_execution_deadline", state(), task=task(started_at=None))
addst("_past_execution_deadline", state(), io={"clock": 5000.0}, task=task(started_at=100.0))
addst("_past_execution_deadline", state(), io={"clock": 100.0}, task=task(started_at=100.0))
addst("_past_execution_deadline", state(), io={"clock": 5000.0}, task=task(started_at="100.5"))
addst("_past_execution_deadline", state(), io={"clock": 5000.0}, task=task(started_at="abc"))
addst("_past_execution_deadline", state(), io={"clock": rs("RuntimeError", "clk")}, task=task(started_at=100.0))
addst("_past_execution_deadline", state(), io={"clock": 5000.0}, task=task(started_at=True))

# ── _cache_lease ─────────────────────────────────────────────────────────────
addst("_cache_lease", state(), lease=lease())
addst("_cache_lease", state(leases={"r1": lease(expires_at=2000.0)}), lease=lease(expires_at=1000.0))
addst("_cache_lease", state(leases={"r1": lease(expires_at=500.0)}), lease=lease(expires_at=1000.0))
addst("_cache_lease", state(leases={"r1": lease(lease_generation=1)}), lease=lease(lease_generation=2))
addst("_cache_lease", state(leases={"r1": lease(process_generation="other")}), lease=lease())
addst("_cache_lease", state(), lease=None)
addst("_cache_lease", state(leases={"r1": lease(expires_at=1000.0)}), lease=lease(expires_at=1000.0))

# ── _drop_lease ──────────────────────────────────────────────────────────────
addst("_drop_lease", state(leases={"r1": lease()}), room_id="r1")
addst("_drop_lease", state(leases={"r1": lease()}), room_id="r9")
addst("_drop_lease", state(), room_id="r1")
addst("_drop_lease", state(leases={"r1": lease(), "r2": lease("r2")}), room_id="r1")
addst("_drop_lease", state(leases={"r\u00e9": lease("r\u00e9")}), room_id="r\u00e9")

# ── _mark_ambiguous ──────────────────────────────────────────────────────────
addst("_mark_ambiguous", state(), binding=binding(), attempt=attempt())
addst("_mark_ambiguous", state(leases={"r1": lease(expires_at=2000.0)}), binding=binding(), attempt=attempt(expires_at=1000.0))
addst("_mark_ambiguous", state(leases={"r1": lease(expires_at=500.0)}), binding=binding(), attempt=attempt(expires_at=1000.0))
addst("_mark_ambiguous", state(leases={"r1": lease(lease_generation=9)}), binding=binding(), attempt=attempt(lease_gen=1))
addst("_mark_ambiguous", state(leases={"r1": lease()}), binding=binding(), attempt=None)
addst("_mark_ambiguous", state(leases={"r1": lease(expires_at=2000.0)}), binding=binding(), attempt=attempt(expires_at=3000.0))

# ── _renew_lease_if_needed ───────────────────────────────────────────────────
addst("_renew_lease_if_needed", state(), io={"clock": 100.0}, lease=lease(expires_at=1000.0))
addst("_renew_lease_if_needed", state(), io={"clock": 990.0, "state.renew_lease": sc(lease(expires_at=2000.0, lease_generation=2))}, lease=lease(expires_at=1000.0))
addst("_renew_lease_if_needed", state(), io={"state.renew_lease": sc(lease(expires_at=2000.0))}, lease=lease(expires_at=1000.0), force=True)
addst("_renew_lease_if_needed", state(leases={"r1": lease(expires_at=2000.0)}), io={"clock": 100.0}, lease=lease(expires_at=1000.0))
addst("_renew_lease_if_needed", state(), io={"clock": rs("RuntimeError", "clk")}, lease=lease())
addst("_renew_lease_if_needed", state(), io={"clock": 990.0, "state.renew_lease": rs("StaleLeaseError", "stale")}, lease=lease())
addst("_renew_lease_if_needed", state(), io={"clock": 990.0, "state.renew_lease": sc(lease(expires_at=500.0))}, lease=lease(expires_at=1000.0))

# ── _ensure_lease ────────────────────────────────────────────────────────────
addst("_ensure_lease", state(leases={"r1": lease(expires_at=1000.0)}), io={"clock": 100.0}, binding=binding())
addst("_ensure_lease", state(leases={"r1": lease(expires_at=1000.0)}), io={"clock": 990.0, "state.renew_lease": sc(lease(expires_at=2000.0))}, binding=binding())
addst("_ensure_lease", state(), io={"state.acquire_lease": sc(lease())}, binding=binding())
addst("_ensure_lease", state(), io={"state.acquire_lease": rs("DriverStateError", "no")}, binding=binding())
addst("_ensure_lease", state(leases={"r1": lease()}), io={"clock": 990.0, "state.renew_lease": rs("StaleLeaseError", "stale"), "state.acquire_lease": sc(lease(lease_generation=2))}, binding=binding())
addst("_ensure_lease", state(recovered=[["r1", 1], ["r2", 1]]), io={"state.acquire_lease": sc(lease())}, binding=binding())
addst("_ensure_lease", state(), binding=None)
addst("_ensure_lease", state(leases={"r1": lease()}), io={"clock": 990.0, "state.renew_lease": rs("StaleTaskError", "task")}, binding=binding())

# ── _release_idle_leases ─────────────────────────────────────────────────────
addst("_release_idle_leases", state())
addst("_release_idle_leases", state(leases={"r1": lease()}), io={"state.release_lease": None})
addst("_release_idle_leases", state(leases={"r1": lease()}), io={"state.release_lease": rs("DriverStateError", "gone")})
addst("_release_idle_leases", state(leases={"r1": lease()}), io={"state.release_lease": rs("RuntimeError", "boom")})
addst("_release_idle_leases", state(leases={"r1": lease(), "r2": lease("r2")}), io={"state.release_lease": [None, rs("StaleLeaseError", "s")]})
addst("_release_idle_leases", state(leases={"r1": lease()}), io={"state.release_lease": rs("SystemExit", "bye")})

# ── _fenced ──────────────────────────────────────────────────────────────────
addst("_fenced", state(cb={"publish_terminal": True}), io={"state.settle_stopping_task": sc(task(status="cancelled")), "publish_terminal": None}, op="settle_stopping_task", binding=binding(), task=task(status="stopping"), lease=lease())
addst("_fenced", state(), io={"state.settle_stopping_task": sc(task())}, op="settle_stopping_task", binding=binding(), task=task(), lease=lease(), publish=False)
addst("_fenced", state(), io={"state.defer_indeterminate_task": sc(task(status="deferred"))}, op="defer_indeterminate_task", binding=None, task=task(), lease=lease(), extra={"reason": "member_unavailable"})
addst("_fenced", state(), io={"state.resolve_indeterminate_task": sc(task())}, op="resolve_indeterminate_task", binding=binding(), task=task(), lease=lease(), extra={"expected_execution_generation": 9})
addst("_fenced", state(), op="settle_stopping_task", binding=binding(), task=task(execution_generation="bad"), lease=lease())
addst("_fenced", state(), io={"state.settle_stopping_task": rs("StaleTaskError", "stale")}, op="settle_stopping_task", binding=binding(), task=task(), lease=lease())
addst("_fenced", state(), op="settle_stopping_task", binding=binding(), task=task(), lease=lease())
addst("_fenced", state(), op="settle_stopping_task", binding=binding(), task={"status": "stopping"}, lease=lease())

# ── _complete_cancel ─────────────────────────────────────────────────────────
addst("_complete_cancel", state(), io={"state.complete_task_cancel": sc(task(status="cancelled"))}, task=task(status="stopping"))
addst("_complete_cancel", state(), io={"state.complete_task_cancel": sc(task(status="cancelled"))}, task=task(), cancel_id="explicit")
addst("_complete_cancel", state(), task={"identity": ident(), "status": "stopping", "cancel_generation": 1})
addst("_complete_cancel", state(), task={"identity": ident(), "status": "stopping", "cancel_id": "c1"})
addst("_complete_cancel", state(), io={"state.complete_task_cancel": rs("InvalidTaskTransitionError", "no")}, task=task())
addst("_complete_cancel", state(), task={"status": "stopping"})
addst("_complete_cancel", state(), io={"state.complete_task_cancel": sc(task())}, task=task(), cancel_id=None)

# ── _requeue ─────────────────────────────────────────────────────────────────
addst("_requeue", state(cb={"retire_task_progress": True}, blocked=["r1"]), io={"state.requeue_deferred_task": sc(task(status="queued")), "retire_task_progress": None}, requeue="requeue_deferred_task", task=task(status="deferred"), lease=lease(), room_id="r1")
addst("_requeue", state(cb={"retire_task_progress": False}), io={"state.requeue_indeterminate_task": sc(task(status="queued"))}, requeue="requeue_indeterminate_task", task=task(), lease=lease(), room_id="r1")
addst("_requeue", state(cb={"retire_task_progress": True}), io={"state.requeue_deferred_task": rs("StaleTaskError", "s")}, requeue="requeue_deferred_task", task=task(), lease=lease(), room_id="r1")
addst("_requeue", state(cb={"retire_task_progress": True}), io={"state.requeue_deferred_task": sc(task()), "retire_task_progress": rs("RuntimeError", "r")}, requeue="requeue_deferred_task", task=task(), lease=lease(), room_id="r1")
addst("_requeue", state(cb={"retire_task_progress": True}), io={"state.requeue_deferred_task": sc(task())}, requeue="requeue_deferred_task", task=task(execution_generation="bad"), lease=lease(), room_id="r1")
addst("_requeue", state(blocked=["r1"], room_threads=["r1"]), io={"state.requeue_deferred_task": sc(task())}, requeue="requeue_deferred_task", task=task(), lease=lease(), room_id="r1")

# ── _resolve_indeterminate ───────────────────────────────────────────────────
TERM = {"status": "settled", "settlement_id": "s1", "result": {"message_id": "m1", "text": "ok"}}
addst("_resolve_indeterminate", state(cb={"publish_terminal": True}), io={"state.resolve_indeterminate_task": sc(task(status="settled")), "publish_terminal": None}, binding=binding(), task=task(status="indeterminate"), lease=lease(), terminal=TERM)
addst("_resolve_indeterminate", state(), io={"state.resolve_indeterminate_task": sc(task())}, binding=binding(), task=task(), lease=lease(), terminal=TERM, publish=False)
addst("_resolve_indeterminate", state(), io={"state.resolve_indeterminate_task": rs("StaleLeaseError", "s")}, binding=binding(), task=task(), lease=lease(), terminal=TERM)
addst("_resolve_indeterminate", state(), io={"state.resolve_indeterminate_task": sc(task())}, binding=None, task=task(), lease=lease(), terminal=TERM)
addst("_resolve_indeterminate", state(), io={"state.resolve_indeterminate_task": sc(task())}, binding=binding(), task=task(), lease=lease(), terminal={"status": "failed", "settlement_id": "s2", "result": {"text": "x", "error": "e"}})

# ── _resolve_or_create ───────────────────────────────────────────────────────
addst("_resolve_or_create", state(), io={"transport.resolve_exact": {"session_id": "s1"}, "transport.resume": {"session_id": "s2"}}, transport={"id": "t1"}, profile="p1", room_id="r1")
addst("_resolve_or_create", state(), io={"transport.resolve_exact": None, "transport.create": {"session_id": "s3"}}, transport={"id": "t1"}, profile="p1", room_id="r1")
addst("_resolve_or_create", state(), io={"transport.resolve_exact": None}, transport={"id": "t1"}, profile="p1", room_id="r1", create=False)
addst("_resolve_or_create", state(), io={"transport.resolve_exact": {"session_id": 5}}, transport={"id": "t1"}, profile="p1", room_id="r1")
addst("_resolve_or_create", state(), io={"transport.resolve_exact": rs("RuntimeError", "rpc")}, transport={"id": "t1"}, profile="p1", room_id="r1")
addst("_resolve_or_create", state(), io={"transport.resolve_exact": None, "transport.create": rs("RuntimeError", "c")}, transport={"id": "t1"}, profile="p1", room_id="r1")
addst("_resolve_or_create", state(), io={"transport.resolve_exact": {"id": "s9"}, "transport.resume": {"session_id": "s2"}}, transport={"id": "t1"}, profile="p1", room_id="r1")

# ── _resume_exact ────────────────────────────────────────────────────────────
addst("_resume_exact", state(), io={"transport.resolve_exact": None}, transport={"id": "t1"}, room_id="r1", profile="p1")
addst("_resume_exact", state(), io={"transport.resolve_exact": {"session_id": "s1"}, "transport.resume": {"session_id": "s2"}}, transport={"id": "t1"}, room_id="r1", profile="p1")
addst("_resume_exact", state(), io={"transport.resolve_exact": {"session_id": "s1"}, "transport.resume": {"id": "s3"}}, transport={"id": "t1"}, room_id="r1", profile="p1")
addst("_resume_exact", state(), io={"transport.resolve_exact": {"session_id": "s1"}, "transport.resume": {"session_id": ""}}, transport={"id": "t1"}, room_id="r1", profile="p1")
addst("_resume_exact", state(), io={"transport.resolve_exact": rs("RuntimeError", "x")}, transport={"id": "t1"}, room_id="r1", profile="p1")
addst("_resume_exact", state(), io={"transport.resolve_exact": {"session_id": None}}, transport={"id": "t1"}, room_id="r1", profile="p1")

# ── _open_session ────────────────────────────────────────────────────────────
addst("_open_session", state(cb={"transport_resolver": True}), io={"transport_resolver": None}, binding=binding(), task=task())
addst("_open_session", state(cb={"transport_resolver": False, "rpc": True}), binding=binding(), task=task(), peer_only=True)
addst("_open_session", state(cb={"transport_resolver": True}), io={"transport_resolver": {"id": "t1"}, "transport.resolve_exact": None}, binding=binding(), task=task(), peer_only=True)
addst("_open_session", state(cb={"transport_resolver": True}), io={"transport_resolver": {"id": "t1"}}, binding=binding(), task={"identity": ident()})
addst("_open_session", state(cb={"transport_resolver": True}), io={"transport_resolver": {"id": "t1"}}, binding=binding(), task=task(payload={}))
addst("_open_session", state(cb={"transport_resolver": True}), io={"transport_resolver": {"id": "t1"}, "transport.resolve_exact": {"session_id": "s1"}, "transport.resume": {"session_id": "s2"}}, binding=binding(), task=task())
addst("_open_session", state(cb={"transport_resolver": True}), io={"transport_resolver": rs("RuntimeError", "x")}, binding=binding(), task=task())
addst("_open_session", state(cb={"transport_resolver": False, "rpc": True}), binding=binding(), task=task())

# ── _terminal_from_history ───────────────────────────────────────────────────
addst("_terminal_from_history", state(), io={"transport.history": sc(HIST)}, transport={"id": "t1"}, profile="p1", session_id="s1", task=task())
addst("_terminal_from_history", state(), io={"transport.history": sc([])}, transport={"id": "t1"}, profile="p1", session_id="s1", task=task())
addst("_terminal_from_history", state(), io={"transport.history": rs("RuntimeError", "h")}, transport={"id": "t1"}, profile="p1", session_id="s1", task=task())
addst("_terminal_from_history", state(), io={"transport.history": sc([])}, transport={"id": "t1"}, profile="p1", session_id="s1", task={"status": "running"})
addst("_terminal_from_history", state(), io={"transport.history": sc([])}, transport={"id": "t1"}, profile="p1", session_id="s1", task=task(execution_generation="4"))
addst("_terminal_from_history", state(), io={"transport.history": sc([])}, transport={"id": "t1"}, profile="p1", session_id="s1", task=task(execution_generation="bad"))

# ── _peer_stop_acknowledged ──────────────────────────────────────────────────
PEER_IO = {"transport_resolver": {"id": "t1"}, "transport.resolve_exact": {"session_id": "s1"}, "transport.resume": {"session_id": "s1"}}
addst("_peer_stop_acknowledged", state(cb={"transport_resolver": True}), io={"transport_resolver": None}, binding=binding(), task=task())
addst("_peer_stop_acknowledged", state(cb={"transport_resolver": True}), io={**PEER_IO, "transport.info": {"active": True, "status": "cancelled", "task_id": "t1", "execution_generation": 3}}, binding=binding(), task=task())
addst("_peer_stop_acknowledged", state(cb={"transport_resolver": True}), io={**PEER_IO, "transport.info": {"active": False, "status": "cancelled", "task_id": "t1", "execution_generation": 3}}, binding=binding(), task=task())
addst("_peer_stop_acknowledged", state(cb={"transport_resolver": True}), io={**PEER_IO, "transport.info": {"active": False, "status": "running", "task_id": "t1", "execution_generation": 3}}, binding=binding(), task=task())
addst("_peer_stop_acknowledged", state(cb={"transport_resolver": True}), io={**PEER_IO, "transport.info": {"active": False, "status": "interrupted", "task_id": "other", "execution_generation": 3}}, binding=binding(), task=task())
addst("_peer_stop_acknowledged", state(cb={"transport_resolver": True}), io={**PEER_IO, "transport.info": {"active": False, "status": "cancelled", "task_id": "t1", "execution_generation": 9}}, binding=binding(), task=task())
addst("_peer_stop_acknowledged", state(cb={"transport_resolver": True}), io={**PEER_IO, "transport.info": rs("RuntimeError", "i")}, binding=binding(), task=task())
addst("_peer_stop_acknowledged", state(cb={"transport_resolver": True}), io={**PEER_IO, "transport.info": {"active": False, "status": "cancelled", "task_id": "t1", "execution_generation": "3"}}, binding=binding(), task=task())
addst("_peer_stop_acknowledged", state(cb={"transport_resolver": True}), io={**PEER_IO, "transport.info": {"active": False, "status": "cancelled", "task_id": "t1", "execution_generation": 3}}, binding=binding(), task=task(execution_generation=None))

# ── _report_pending_action ───────────────────────────────────────────────────
addst("_report_pending_action", state(cb={"task_progress": True, "pending_action": True}), io={"task_progress": None, "pending_action": None}, task=task(), session_id="s1", info={"pending_approval": {"request_id": "q1", "choices": ["once", "deny", "always"]}, "run_id": "run1"})
addst("_report_pending_action", state(cb={"task_progress": True, "pending_action": True}), io={"task_progress": None, "pending_action": None}, task=task(), session_id="s1", info={})
addst("_report_pending_action", state(cb={"task_progress": True, "pending_action": True}), io={"task_progress": None, "pending_action": None}, task=task(), session_id="s1", info={"approval": {"choices": []}})
addst("_report_pending_action", state(cb={"task_progress": True, "pending_action": True}), io={"task_progress": None, "pending_action": None}, task=task(), session_id="s1", info={"pending_approval": {"choices": [["x"]]}})
addst("_report_pending_action", state(cb={"task_progress": True, "pending_action": False}), io={"task_progress": None}, task=task(), session_id="s1", info={"pending_approval": {"choices": ["once"]}})
addst("_report_pending_action", state(cb={"task_progress": True, "pending_action": True}), io={"task_progress": rs("RuntimeError", "p")}, task=task(), session_id="s1", info={})
addst("_report_pending_action", state(cb={"task_progress": False, "pending_action": True}), io={"pending_action": None}, task=task(), session_id="s1", info={"pending_approval": "notamapping"})
addst("_report_pending_action", state(cb={"task_progress": True, "pending_action": True}), io={"task_progress": None, "pending_action": None}, task=task(payload={"target_profile": "p1"}), session_id="s1", info={"pending_approval": {"choices": "once"}})

# ── _served_elsewhere ────────────────────────────────────────────────────────
addst("_served_elsewhere", state(cb={"remote_member": True}), io={"remote_member": True}, binding=binding(), task=task())
addst("_served_elsewhere", state(cb={"remote_member": True, "transport_resolver": True}), io={"remote_member": False, "transport_resolver": {"id": "t1", "served_elsewhere": True}}, binding=binding(), task=task())
addst("_served_elsewhere", state(cb={"remote_member": False, "transport_resolver": True}), io={"transport_resolver": {"id": "t1", "served_elsewhere": False}}, binding=binding(), task=task())
addst("_served_elsewhere", state(cb={"remote_member": False, "transport_resolver": True}), io={"transport_resolver": rs("RuntimeError", "gone")}, binding=binding(), task=task())
addst("_served_elsewhere", state(cb={"remote_member": False, "transport_resolver": True}), io={"transport_resolver": rs("SystemExit", "bye")}, binding=binding(), task=task())
addst("_served_elsewhere", state(cb={"remote_member": False, "transport_resolver": True}), io={"transport_resolver": None}, binding=binding(), task=task())
addst("_served_elsewhere", state(cb={"remote_member": True}), io={"remote_member": rs("RuntimeError", "r")}, binding=binding(), task=task())

# ── _defer_unconfirmed_stop ──────────────────────────────────────────────────
addst("_defer_unconfirmed_stop", state(cb={"remote_member": False, "transport_resolver": True}), io={"transport_resolver": {"id": "t1", "served_elsewhere": False}}, binding=binding(), task=task(), lease=lease())
addst("_defer_unconfirmed_stop", state(cb={"remote_member": True}), io={"remote_member": True, "clock": 100.0}, binding=binding(), task=task(updated_at=100.0), lease=lease())
addst("_defer_unconfirmed_stop", state(cb={"remote_member": True, "publish_terminal": True}), io={"remote_member": True, "clock": 500.0, "state.acquire_lease": sc(lease()), "state.defer_unconfirmed_stop": sc(task(status="deferred")), "publish_terminal": None}, binding=binding(), task=task(updated_at=100.0), lease=lease())
addst("_defer_unconfirmed_stop", state(cb={"remote_member": True}), io={"remote_member": True, "clock": 500.0, "state.acquire_lease": sc(lease()), "state.defer_unconfirmed_stop": sc(task())}, binding=binding(), task=task(updated_at="100.0"), lease=lease())
addst("_defer_unconfirmed_stop", state(cb={"remote_member": True}), io={"remote_member": True, "clock": 500.0}, binding=binding(), task=task(updated_at="abc"), lease=lease())
addst("_defer_unconfirmed_stop", state(cb={"remote_member": True}), io={"remote_member": True, "clock": rs("RuntimeError", "clk")}, binding=binding(), task=task(), lease=lease())
addst("_defer_unconfirmed_stop", state(cb={"remote_member": True}), io={"remote_member": True, "clock": 500.0, "state.acquire_lease": sc(lease()), "state.defer_unconfirmed_stop": rs("StaleTaskError", "s")}, binding=binding(), task=task(updated_at=100.0), lease=lease())
addst("_defer_unconfirmed_stop", state(cb={"remote_member": True, "publish_terminal": True}), io={"remote_member": True, "clock": 500.0, "state.acquire_lease": sc(lease()), "state.defer_unconfirmed_stop": sc(task()), "publish_terminal": rs("RuntimeError", "p")}, binding=binding(), task=task(updated_at=100.0), lease=lease())

# ── _complete_acknowledged_stop ──────────────────────────────────────────────
addst("_complete_acknowledged_stop", state(), io={"state.complete_task_cancel": sc(task(status="cancelled"))}, binding=binding(), task=task(cancel_id="user-stop"), lease=lease())
addst("_complete_acknowledged_stop", state(cb={"publish_terminal": True}), io={"state.settle_stopping_task": sc(task(status="failed")), "publish_terminal": None}, binding=binding(), task=task(cancel_id="deadline:3"), lease=lease())
addst("_complete_acknowledged_stop", state(), io={"state.settle_stopping_task": sc(task())}, binding=binding(), task=task(cancel_id="deadline:3", execution_generation="4"), lease=lease())
addst("_complete_acknowledged_stop", state(), io={"state.complete_task_cancel": sc(task())}, binding=binding(), task=task(cancel_id=None), lease=lease())
addst("_complete_acknowledged_stop", state(), io={"state.complete_task_cancel": sc(task())}, binding=binding(), task=task(cancel_id=5), lease=lease())
addst("_complete_acknowledged_stop", state(), io={"state.settle_stopping_task": rs("StaleLeaseError", "s")}, binding=binding(), task=task(cancel_id="deadline:3"), lease=lease())
addst("_complete_acknowledged_stop", state(), binding=binding(), task=task(cancel_id="deadline:3", execution_generation=None), lease=lease())

# ── _settle_failure_if_current ───────────────────────────────────────────────
addst("_settle_failure_if_current", state(), io={"state.settle_task": sc(task(status="failed"))}, attempt=attempt(), exc=rs("RuntimeError", "boom"))
addst("_settle_failure_if_current", state(), io={"state.settle_task": rs("DriverStateError", "malformed")}, attempt=attempt(), exc=rs("RuntimeError", "boom"))
addst("_settle_failure_if_current", state(), io={"state.settle_task": rs("RuntimeError", "other")}, attempt=attempt(), exc=rs("RuntimeError", "boom"))
addst("_settle_failure_if_current", state(), io={"state.settle_task": rs("StaleLeaseError", "s")}, attempt=attempt(), exc=rs("ValueError", "bad"))
addst("_settle_failure_if_current", state(), attempt=None, exc=rs("RuntimeError", "boom"))
addst("_settle_failure_if_current", state(last_error="old"), io={"state.settle_task": None}, attempt=attempt(task_id="t9", execution_generation=7), exc=rs("RuntimeError", "credential leak"))
addst("_settle_failure_if_current", state(), io={"state.settle_task": rs("SystemExit", "bye")}, attempt=attempt(), exc=rs("RuntimeError", "boom"))


# ── callback-return discard probes (live ignores these returns) ──────────────
addst("_report_pending_action", state(cb={"task_progress": True, "pending_action": True}), io={"task_progress": {"ignored": 1}, "pending_action": {"ignored": 2}}, task=task(), session_id="s1", info={})
addst("_publish", state(cb={"publish_terminal": True}), io={"publish_terminal": {"ignored": 1}}, binding=binding(), task=task())
addst("_requeue", state(cb={"retire_task_progress": True}), io={"state.requeue_deferred_task": sc(task()), "retire_task_progress": {"ignored": 1}}, requeue="requeue_deferred_task", task=task(), lease=lease(), room_id="r1")


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-func coverage report
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    low = {k: v for k, v in counts.items() if v < 5}
    print("wrote %d cases for %d funcs -> %s" % (len(CASES), len(counts), OUT))
    if low:
        print("UNDER-COVERED:", low)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
