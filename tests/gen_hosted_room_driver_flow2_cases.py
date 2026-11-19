#!/usr/bin/env python3
"""Generate tests/fixtures/hosted_room_driver_flow2.jsonl for the UNCLAIMED
stop/cancel/recovery remainder of tui_gateway/hosted_room_driver.py
(hosted_room_driver_flow2).

Deterministic, hand-picked branch/edge/raise/fail-closed cases: >=5 per ported
function. Shapes match the harness contract:
  {"func": <py name>, "in": {"state": {...}, "io": {...}, ...args}}
io scripts: bare value = constant; bare array = per-call queue;
{"__script__": true, "ret": v} = constant list/object; a missing/exhausted
script is fail-closed (RuntimeError spec) on BOTH sides. Claimed siblings ride
"self.<name>" seams; state ops "state.<name>"; transport "transport.<name>".
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "hosted_room_driver_flow2.jsonl")

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


def task(**kw):
    base = {"identity": ident(), "status": "stopping", "execution_generation": 3,
            "cancel_generation": 1, "cancel_id": "c1", "updated_at": 100.0,
            "run_process_generation": "pg-old", "run_gateway_id": "g0", "run_lease_generation": 0,
            "payload": {"target_profile": "p1", "target_member_id": "m1", "prompt": "hi"}}
    base.update(kw)
    return base


def receipt(**kw):
    base = {"status": "settled", "settlement_id": "s1",
            "result": {"message_id": None, "text": "ok"}}
    base.update(kw)
    return base


def tp(tid="peer", discard=False):
    d = {"id": tid}
    if discard:
        d["discard"] = True
    return d


def opened(transport=None, profile="p1", sid="s1"):
    return [transport if transport is not None else tp(), profile, sid]


def sc(ret):
    return {"__script__": True, "ret": ret}


def rs(kind, msg=""):
    return {"__raise__": kind, "message": msg}


def st(**kw):
    return kw


CASES = []


def add(func, io=None, **args):
    CASES.append({"func": func, "in": {"io": io or {}, **args}})


def addst(func, state, io=None, **args):
    CASES.append({"func": func, "in": {"state": state, "io": io or {}, **args}})


B = binding()
L = lease()
T = task()

# ── _stop_outcome ────────────────────────────────────────────────────────────
add("_stop_outcome", {"self._open_session": sc(opened(tp("rpc"), sid=None))}, binding=B, task=T)
add("_stop_outcome", {"self._open_session": sc(opened(tp("peer"), sid=None))}, binding=B, task=T)
add("_stop_outcome", {"self._open_session": sc(opened(None, sid=None))}, binding=B, task=T)
add("_stop_outcome", {"self._open_session": sc(opened()), "transport.info": sc({"active": False})},
    binding=B, task=T)
add("_stop_outcome", {"self._open_session": sc(opened()),
                      "transport.info": sc({"active": True, "task_id": "t1"}),
                      "transport.interrupt": sc({"interrupted": True})}, binding=B, task=T)
add("_stop_outcome", {"self._open_session": sc(opened()),
                      "transport.info": sc({"active": True, "task_id": "t1"}),
                      "transport.interrupt": sc({"status": "cancelled"})}, binding=B, task=T)
add("_stop_outcome", {"self._open_session": sc(opened()),
                      "transport.info": sc({"active": True, "task_id": "t1"}),
                      "transport.interrupt": sc(None)}, binding=B, task=T)
add("_stop_outcome", {"self._open_session": sc(opened()),
                      "transport.info": sc({"active": True, "task_id": "other"}),
                      "self._tasks": sc([])}, binding=B, task=T)
add("_stop_outcome", {"self._open_session": sc(opened(tp("peer", True))),
                      "transport.info": sc({"status": "unknown", "active": False, "task_id": "t1",
                                            "execution_generation": 3}),
                      "transport.discard": sc({"discarded": True, "task_id": "t1",
                                               "execution_generation": 3, "status": "cancelled"})},
    binding=B, task=task(status="stopping"))
add("_stop_outcome", {}, binding=B, task=T)   # fail-closed

# ── _discard_unknown_stop ────────────────────────────────────────────────────
DI = {"active": False, "task_id": "t1", "execution_generation": 3}
add("_discard_unknown_stop", {}, transport=tp("peer", True), profile="p1", session_id="s1",
    task=T, info={"active": True, "task_id": "t1", "execution_generation": 3})
add("_discard_unknown_stop", {}, transport=tp("peer", True), profile="p1", session_id="s1",
    task=T, info={**DI, "task_id": "other"})
add("_discard_unknown_stop", {}, transport=tp("peer", True), profile="p1", session_id="s1",
    task=T, info={**DI, "execution_generation": "3"})
add("_discard_unknown_stop", {}, transport=tp("peer", True), profile="p1", session_id="s1",
    task=T, info={**DI, "execution_generation": 4})
add("_discard_unknown_stop", {"transport.discard": sc({"discarded": True, "task_id": "t1",
                                                       "execution_generation": 3, "status": "settled",
                                                       "settlement_id": "sx", "text": "hi"})},
    transport=tp("peer", True), profile="p1", session_id="s1", task=T, info=DI)
add("_discard_unknown_stop", {"transport.discard": sc({"discarded": False})},
    transport=tp("peer", True), profile="p1", session_id="s1", task=T, info=DI)
add("_discard_unknown_stop", {"transport.discard": sc({"discarded": True, "task_id": "t1",
                                                       "execution_generation": 3, "status": "cancelled"})},
    transport=tp("peer", True), profile="p1", session_id="s1", task=T, info=DI)
add("_discard_unknown_stop", {"transport.discard": sc(None)},
    transport=tp("peer", True), profile="p1", session_id="s1", task=T, info=DI)
add("_discard_unknown_stop", {}, transport=tp("peer", True), profile="p1", session_id="s1",
    task=T, info=DI)   # fail-closed (no discard script)

# ── _interrupt_stale_admission ───────────────────────────────────────────────
add("_interrupt_stale_admission", {}, transport=tp(), binding=B, profile="p1", session_id="s1",
    info={"task_id": ""})
add("_interrupt_stale_admission", {"self._tasks": sc([task(identity=ident(task_id="t1"))])},
    transport=tp(), binding=B, profile="p1", session_id="s1", info={"task_id": "t1"})
add("_interrupt_stale_admission", {"self._tasks": sc([])},
    transport=tp(), binding=B, profile="p1", session_id="s1", info={"task_id": "t9"})
add("_interrupt_stale_admission", {"self._tasks": [[], [], [task(identity=ident(task_id="t9"))]],
                                   "self._remote": sc(True)},
    transport=tp(), binding=B, profile="p1", session_id="s1", info={"task_id": "t9"})
add("_interrupt_stale_admission", {"self._tasks": [[], [], [task(identity=ident(task_id="t9"))]],
                                   "self._remote": sc(False), "transport.interrupt": sc(None)},
    transport=tp(), binding=B, profile="p1", session_id="s1", info={"task_id": "t9"})
add("_interrupt_stale_admission", {"self._tasks": [[], [], [task(identity=ident(task_id="t9"))]],
                                   "self._remote": sc(False), "transport.interrupt": sc(rs("RuntimeError", "x"))},
    transport=tp(), binding=B, profile="p1", session_id="s1", info={"task_id": "t9"})
add("_interrupt_stale_admission", {"self._tasks": [[], [], [task(identity=ident(task_id="t9"))]],
                                   "self._remote": sc(False), "transport.interrupt": sc(rs("KeyError", "'k'"))},
    transport=tp(), binding=B, profile="p1", session_id="s1", info={"task_id": "t9"})
add("_interrupt_stale_admission", {}, transport=tp(), binding=B, profile="p1", session_id="s1",
    info={"task_id": "t9"})   # fail-closed

# ── _interrupt_stopping_task ─────────────────────────────────────────────────
add("_interrupt_stopping_task", {"self._open_session": sc(opened(tp("rpc"), sid=None))}, binding=B, task=T)
add("_interrupt_stopping_task", {"self._open_session": sc(opened(tp("peer"), sid=None))}, binding=B, task=T)
add("_interrupt_stopping_task", {"self._open_session": sc(opened(tp("peer", True))),
                                 "transport.info": sc({"status": "unknown", "active": False, "task_id": "t1",
                                                       "execution_generation": 3}),
                                 "transport.discard": sc({"discarded": True, "task_id": "t1",
                                                          "execution_generation": 3, "status": "settled",
                                                          "settlement_id": "sx", "text": "hi"})},
    binding=B, task=task(status="stopping"))
add("_interrupt_stopping_task", {"self._open_session": sc(opened()), "transport.info": sc({"active": False})},
    binding=B, task=T)
add("_interrupt_stopping_task", {}, binding=B, task=T)   # fail-closed

# ── _stop_or_settle ──────────────────────────────────────────────────────────
add("_stop_or_settle", {"self._open_session": sc(opened(tp("rpc"), sid=None))}, binding=B, task=T, lease=L)
add("_stop_or_settle", {"self._open_session": sc(opened(tp("peer"), sid=None))}, binding=B, task=T, lease=L)
add("_stop_or_settle", {"self._open_session": sc(opened(tp("peer", True))),
                        "transport.info": sc({"status": "unknown", "active": False, "task_id": "t1",
                                              "execution_generation": 3}),
                        "transport.discard": sc({"discarded": True, "task_id": "t1",
                                                 "execution_generation": 3, "status": "settled",
                                                 "settlement_id": "sx", "text": "hi"}),
                        "self._fenced": sc(task())}, binding=B, task=task(status="stopping"), lease=L)
add("_stop_or_settle", {}, binding=B, task=T, lease=L)   # fail-closed
add("_stop_or_settle", {"self._open_session": sc(opened(tp("peer", True))),
                        "transport.info": sc({"status": "unknown", "active": False, "task_id": "t1",
                                              "execution_generation": 3}),
                        "transport.discard": sc({"discarded": True, "task_id": "t1",
                                                 "execution_generation": 3, "status": "settled",
                                                 "settlement_id": "sx", "text": "hi"}),
                        "self._fenced": sc(rs("StaleLeaseError", "x"))},
    binding=B, task=task(status="stopping"), lease=L)

# ── _finish_stop ─────────────────────────────────────────────────────────────
add("_finish_stop", {"self._open_session": sc(opened()),
                     "self._terminal_from_history": sc(receipt()), "self._fenced": sc(task())},
    binding=B, task=T, lease=L)
add("_finish_stop", {"self._open_session": sc(opened(tp("rpc"), sid=None)),
                     "self._complete_acknowledged_stop": sc(task())}, binding=B, task=T, lease=L)
add("_finish_stop", {"self._open_session": sc(rs("StaleLeaseError", "x"))}, binding=B, task=T, lease=L)
add("_finish_stop", {"self._open_session": [rs("RuntimeError", "boom"), opened(tp("rpc"), sid=None)],
                     "self._record_error": sc(None), "self._complete_acknowledged_stop": sc(task())},
    binding=B, task=T, lease=L)
add("_finish_stop", {"self._open_session": sc(opened(tp("peer"), sid=None))}, binding=B, task=T, lease=L)
add("_finish_stop", {"self._open_session": [opened(tp("peer"), sid=None), opened(tp("peer2", True))],
                     "transport.info": sc({"status": "unknown", "active": False, "task_id": "t1",
                                           "execution_generation": 3}),
                     "transport.discard": sc({"discarded": True, "task_id": "t1",
                                              "execution_generation": 3, "status": "settled",
                                              "settlement_id": "sx", "text": "hi"}),
                     "self._fenced": sc(task())}, binding=B, task=task(status="stopping"), lease=L)
add("_finish_stop", {}, binding=B, task=T, lease=L)   # fail-closed

# ── _settle_stopping_completion ──────────────────────────────────────────────
add("_settle_stopping_completion", {"self._open_session": sc(opened(tp("rpc"), sid=None))},
    binding=B, task=T, lease=L)
add("_settle_stopping_completion", {"self._open_session": sc(opened()),
                                    "self._terminal_from_history": sc(None)}, binding=B, task=T, lease=L)
add("_settle_stopping_completion", {"self._open_session": sc(opened()),
                                    "self._terminal_from_history": sc(receipt()), "self._fenced": sc(task())},
    binding=B, task=T, lease=L)
add("_settle_stopping_completion", {"self._open_session": sc(opened()),
                                    "self._terminal_from_history": sc(receipt()),
                                    "self._fenced": sc(rs("StaleTaskError", "x"))}, binding=B, task=T, lease=L)
add("_settle_stopping_completion", {"self._open_session": sc(rs("RuntimeError", "x"))},
    binding=B, task=T, lease=L)
add("_settle_stopping_completion", {}, binding=B, task=T, lease=L)   # fail-closed

# ── _retry_stopping_tasks ────────────────────────────────────────────────────
add("_retry_stopping_tasks", {"self._tasks": sc([])}, binding=B, lease=L)
add("_retry_stopping_tasks", {"self._tasks": sc([task()]), "self._renew_lease_if_needed": sc(lease()),
                              "self._peer_stop_acknowledged": sc(True), "self._complete_cancel": sc(task())},
    binding=B, lease=L)
add("_retry_stopping_tasks", {"self._tasks": sc([task()]), "self._renew_lease_if_needed": sc(lease()),
                              "self._peer_stop_acknowledged": sc(False),
                              "self._open_session": sc(opened(tp("rpc"), sid=None)),
                              "self._complete_acknowledged_stop": sc(task())}, binding=B, lease=L)
add("_retry_stopping_tasks", {"self._tasks": sc([task()]), "self._renew_lease_if_needed": sc(lease()),
                              "self._peer_stop_acknowledged": sc(False),
                              "self._open_session": sc(opened(tp("peer"), sid=None)),
                              "self._defer_unconfirmed_stop": sc(False)}, binding=B, lease=L)
add("_retry_stopping_tasks", {"self._tasks": sc([task()]), "self._renew_lease_if_needed": sc(lease()),
                              "self._peer_stop_acknowledged": sc(False),
                              "self._open_session": sc(opened(tp("peer"), sid=None)),
                              "self._defer_unconfirmed_stop": sc(True)}, binding=B, lease=L)
add("_retry_stopping_tasks", {"self._tasks": sc([task()]), "self._renew_lease_if_needed": sc(rs("RuntimeError", "x")),
                              "self._record_error": sc(None), "self._defer_unconfirmed_stop": sc(False)},
    binding=B, lease=L)
add("_retry_stopping_tasks", {}, binding=B, lease=L)   # fail-closed

# ── cancel ───────────────────────────────────────────────────────────────────
add("cancel", {"state.get_task": sc(task(status="cancelled"))}, identity=ident(), cancel_id="c1")
add("cancel", {"state.get_task": sc(task(status="settled"))}, identity=ident(), cancel_id="c1")
add("cancel", {"state.get_task": sc(task(status="queued")), "state.cancel_task": sc(task(status="cancelled"))},
    identity=ident(), cancel_id="c1")
addst("cancel", st(cb={"confirm_deferred_stop": True}),
      {"state.get_task": sc(task(status="deferred")), "confirm_deferred_stop": sc(False)},
      identity=ident(), cancel_id="c1")
addst("cancel", st(cb={"confirm_deferred_stop": True}),
      {"state.get_task": sc(task(status="deferred")), "confirm_deferred_stop": sc(True),
       "state.cancel_task": sc(task(status="cancelled"))}, identity=ident(), cancel_id="c1")
add("cancel", {"state.get_task": [task(status="running"), task(status="stopping")],
               "state.begin_task_cancel": sc(task(status="stopping")), "self._binding_for_room": sc(None)},
    identity=ident(), cancel_id="c1")
add("cancel", {"state.get_task": [task(status="running"), task(status="stopping")],
               "state.begin_task_cancel": sc(task(status="stopping")), "self._binding_for_room": sc(binding()),
               "self._ensure_lease": sc(lease()), "self._peer_stop_acknowledged": sc(True),
               "self._complete_cancel": sc(task())}, identity=ident(), cancel_id="c1")
add("cancel", {"state.get_task": sc(task(status="stopping", cancel_id="cj")),
               "state.begin_task_cancel": sc(task(status="stopping")), "self._binding_for_room": sc(None)},
    identity=ident(), cancel_id="c1")
add("cancel", {"state.get_task": [task(status="running"), task(status="queued")],
               "state.begin_task_cancel": sc(rs("StaleTaskError", "x")),
               "state.cancel_task": sc(task(status="cancelled"))}, identity=ident(), cancel_id="c1")
add("cancel", {"state.get_task": sc(task(status="running")),
               "state.begin_task_cancel": sc(rs("StaleTaskError", "x"))}, identity=ident(), cancel_id="c1")
add("cancel", {}, identity=ident(), cancel_id="c1")   # fail-closed

# ── retry_indeterminate ──────────────────────────────────────────────────────
add("retry_indeterminate", {"state.get_task": sc(task(execution_generation=3))},
    identity=ident(), execution_generation=4)
add("retry_indeterminate", {"state.get_task": sc(task(status="running"))},
    identity=ident(), execution_generation=None)
add("retry_indeterminate", {"state.get_task": sc(task(status="indeterminate")),
                            "self._binding_for_room": sc(None)}, identity=ident(), execution_generation=None)
add("retry_indeterminate", {"state.get_task": sc(task(status="indeterminate")),
                            "self._binding_for_room": sc(binding()), "self._ensure_lease": sc(lease()),
                            "self._transport_for": sc(tp()), "turn_lock": sc({}),
                            "self._resume_exact": sc("s1"), "self._terminal_from_history": sc(receipt()),
                            "transport.info": sc({"active": False}), "self._report_pending_action": sc(None),
                            "self._resolve_indeterminate": sc(task())}, identity=ident(), execution_generation=None)
add("retry_indeterminate", {"state.get_task": sc(task(status="indeterminate")),
                            "self._binding_for_room": sc(binding()), "self._ensure_lease": sc(lease()),
                            "self._transport_for": sc(tp()), "turn_lock": sc({}),
                            "self._resume_exact": sc("s1"), "self._terminal_from_history": sc(None),
                            "transport.info": sc({"active": False, "status": "cancelled"}),
                            "self._report_pending_action": sc(None), "self._fenced": sc(task())},
    identity=ident(), execution_generation=None)
add("retry_indeterminate", {"state.get_task": sc(task(status="indeterminate")),
                            "self._binding_for_room": sc(binding()), "self._ensure_lease": sc(lease()),
                            "self._transport_for": sc(tp()), "turn_lock": sc({}),
                            "self._resume_exact": sc("s1"), "self._terminal_from_history": sc(None),
                            "transport.info": sc({"active": True, "task_id": "t1"}),
                            "self._report_pending_action": sc(None), "self._set_blocked": sc(None)},
    identity=ident(), execution_generation=None)
add("retry_indeterminate", {"state.get_task": sc(task(status="indeterminate")),
                            "self._binding_for_room": sc(binding()), "self._ensure_lease": sc(lease()),
                            "self._transport_for": sc(tp()), "turn_lock": sc({}),
                            "self._resume_exact": sc("s1"), "self._terminal_from_history": sc(None),
                            "transport.info": sc({"active": False}), "self._report_pending_action": sc(None),
                            "self._requeue": sc(task())}, identity=ident(), execution_generation=None)
add("retry_indeterminate", {"state.get_task": sc(task(status="deferred")),
                            "self._binding_for_room": sc(binding()), "self._ensure_lease": sc(lease()),
                            "self._remote": sc(False), "self._requeue": sc(task())},
    identity=ident(), execution_generation=None)
add("retry_indeterminate", {}, identity=ident(), execution_generation=None)   # fail-closed

# ── _retry_deferred ──────────────────────────────────────────────────────────
add("_retry_deferred", {"self._remote": sc(False), "self._requeue": sc(task())}, binding=B, task=T, lease=L)
add("_retry_deferred", {"self._remote": sc(True), "self._transport_for": sc(rs("RuntimeError", "x"))},
    binding=B, task=T, lease=L)
add("_retry_deferred", {"self._remote": sc(True), "self._transport_for": sc(tp()),
                        "self._ensure_lease": sc(lease()), "self._fenced": sc(task()),
                        "turn_lock": sc({}), "self._resume_exact": sc("s1"),
                        "self._terminal_from_history": sc(receipt()), "transport.info": sc({"active": False}),
                        "self._report_pending_action": sc(None), "self._resolve_indeterminate": sc(task())},
    binding=B, task=T, lease=L)
add("_retry_deferred", {"self._remote": sc(True), "self._transport_for": [tp(), rs("RuntimeError", "boom")],
                        "self._ensure_lease": sc(lease()), "self._fenced": sc(task()),
                        "self._record_error": sc(None)}, binding=B, task=T, lease=L)
add("_retry_deferred", {"self._remote": sc(True), "self._transport_for": sc(tp()),
                        "self._ensure_lease": sc(lease()), "self._fenced": sc(task()),
                        "turn_lock": sc({}), "self._resume_exact": sc("s1"),
                        "self._terminal_from_history": sc(None),
                        "transport.info": sc({"active": False, "status": "cancelled"}),
                        "self._report_pending_action": sc(None), "self._fenced": sc(task())},
    binding=B, task=T, lease=L)
add("_retry_deferred", {"self._remote": sc(True), "self._transport_for": sc(tp()),
                        "self._ensure_lease": sc(lease()), "self._fenced": sc(task()),
                        "turn_lock": sc({}), "self._resume_exact": sc("s1"),
                        "self._terminal_from_history": sc(None), "transport.info": sc({"active": False}),
                        "self._report_pending_action": sc(None)}, binding=B, task=T, lease=L)
add("_retry_deferred", {}, binding=B, task=T, lease=L)   # fail-closed

# ── deferred_stop_confirmed ──────────────────────────────────────────────────
add("deferred_stop_confirmed", {"self._binding_for_room": sc(None)}, room_id="r1", task=T)
add("deferred_stop_confirmed", {"self._binding_for_room": sc(binding()), "self._served_elsewhere": sc(False)},
    room_id="r1", task=T)
add("deferred_stop_confirmed", {"self._binding_for_room": sc(binding()), "self._served_elsewhere": sc(True),
                                "self._open_session": sc(opened(tp("peer"), sid=None))}, room_id="r1", task=T)
add("deferred_stop_confirmed", {"self._binding_for_room": sc(binding()), "self._served_elsewhere": sc(True),
                                "self._open_session": sc(opened(None, sid=None))}, room_id="r1", task=T)
add("deferred_stop_confirmed", {"self._binding_for_room": sc(binding()), "self._served_elsewhere": sc(True),
                                "self._open_session": [opened(tp("peer")), opened(tp("rpc"), sid=None)]},
    room_id="r1", task=T)
add("deferred_stop_confirmed", {"self._binding_for_room": sc(binding()), "self._served_elsewhere": sc(True),
                                "self._open_session": sc(rs("RuntimeError", "x")), "self._record_error": sc(None)},
    room_id="r1", task=T)
add("deferred_stop_confirmed", {}, room_id="r1", task=T)   # fail-closed

# ── _expire_attempt_deadline ─────────────────────────────────────────────────
add("_expire_attempt_deadline", {}, binding=B, task=task(status="queued"), lease=L)
add("_expire_attempt_deadline", {"state.begin_task_cancel": sc(task(status="stopping", cancel_id="deadline:3")),
                                 "self._renew_lease_if_needed": sc(lease()),
                                 "self._open_session": sc(opened(tp("rpc"), sid=None))},
    binding=B, task=task(status="running"), lease=L)
add("_expire_attempt_deadline", {}, binding=B, task=task(status="stopping", cancel_id="c1"), lease=L)
add("_expire_attempt_deadline", {"self._renew_lease_if_needed": sc(lease()),
                                 "self._open_session": sc(opened(tp("peer"), sid=None)),
                                 "self._record_error": sc(None)},
    binding=B, task=task(status="stopping", cancel_id="deadline:3"), lease=L)
add("_expire_attempt_deadline", {"state.begin_task_cancel": sc(task(status="stopping", cancel_id="deadline:3")),
                                 "self._renew_lease_if_needed": sc(lease()),
                                 "self._open_session": sc(opened(tp("rpc"), sid=None))},
    binding=B, task=task(status="indeterminate"), lease=L)
add("_expire_attempt_deadline", {"state.begin_task_cancel": sc(rs("StaleTaskError", "x"))},
    binding=B, task=task(status="running"), lease=L)
add("_expire_attempt_deadline", {}, binding=B, task=task(status="running"), lease=L)   # fail-closed

# ── _inspect_session ─────────────────────────────────────────────────────────
add("_inspect_session", {"transport.info": sc({"active": False}), "self._report_pending_action": sc(None)},
    transport=tp(), task=T, session_id="s1", read_history=False)
add("_inspect_session", {"self._terminal_from_history": sc(receipt()),
                         "transport.info": sc({"active": True, "task_id": "t1", "status": "running"}),
                         "self._report_pending_action": sc(None)},
    transport=tp(), task=T, session_id="s1", read_history=True)
add("_inspect_session", {"transport.info": sc({"active": False, "status": "cancelled"}),
                         "self._report_pending_action": sc(None)},
    transport=tp(), task=T, session_id="s1", read_history=False)
add("_inspect_session", {"transport.info": sc({"active": False, "status": ""}),
                         "self._report_pending_action": sc(None)},
    transport=tp(), task=T, session_id="s1", read_history=False)
add("_inspect_session", {"transport.info": sc(rs("RuntimeError", "x"))},
    transport=tp(), task=T, session_id="s1", read_history=False)
add("_inspect_session", {"transport.info": sc({"active": False}),
                         "self._report_pending_action": sc(rs("RuntimeError", "x"))},
    transport=tp(), task=T, session_id="s1", read_history=False)
add("_inspect_session", {}, transport=tp(), task=T, session_id="s1", read_history=False)   # fail-closed

# ── _inspect_recovery_session ────────────────────────────────────────────────
addst("_inspect_recovery_session", st(cb={"task_progress": True}),
      {"self._transport_for": sc(tp()), "turn_lock": sc({}), "self._resume_exact": sc(None),
       "task_progress": sc(None)}, binding=B, task=T)
add("_inspect_recovery_session", {"self._transport_for": sc(tp()), "turn_lock": sc({}),
                                  "self._resume_exact": sc("s1"), "self._terminal_from_history": sc(None),
                                  "transport.info": sc({"active": False}), "self._report_pending_action": sc(None)},
    binding=B, task=T)
addst("_inspect_recovery_session", st(cb={"task_progress": True}),
      {"self._transport_for": sc(rs("RuntimeError", "x")), "task_progress": sc(None)}, binding=B, task=T)
addst("_inspect_recovery_session", st(cb={"task_progress": True}),
      {"self._transport_for": sc(tp()), "turn_lock": sc({"__exit__": rs("RuntimeError", "exit")}),
       "self._resume_exact": sc(None), "task_progress": sc(None)}, binding=B, task=T)
add("_inspect_recovery_session", {"self._transport_for": sc(tp()), "turn_lock": sc({}),
                                  "self._resume_exact": sc(None)}, binding=B, task=T)
add("_inspect_recovery_session", {}, binding=B, task=T)   # fail-closed

# ── _inspect_local_recovery_session ──────────────────────────────────────────
add("_inspect_local_recovery_session", {"turn_lock": sc({}), "transport.resolve_exact": sc(None)}, task=T)
add("_inspect_local_recovery_session", {"turn_lock": sc({}),
                                        "transport.resolve_exact": sc({"session_id": "s1"}),
                                        "transport.info": sc({"active": False}),
                                        "self._report_pending_action": sc(None)}, task=T)
addst("_inspect_local_recovery_session", st(cb={"rpc": False}), {"turn_lock": sc({})}, task=T)
add("_inspect_local_recovery_session", {"turn_lock": sc({}),
                                        "transport.resolve_exact": sc(rs("RuntimeError", "x"))}, task=T)
add("_inspect_local_recovery_session", {"turn_lock": sc({}),
                                        "transport.resolve_exact": sc({"session_id": ""})}, task=T)
add("_inspect_local_recovery_session", {"turn_lock": sc({"__exit__": rs("RuntimeError", "exit")}),
                                        "transport.resolve_exact": sc(None)}, task=T)
add("_inspect_local_recovery_session", {}, task=T)   # fail-closed

# ── _inspect_abandoned_attempts ──────────────────────────────────────────────
add("_inspect_abandoned_attempts", {"self._tasks": sc([])}, binding=B)
add("_inspect_abandoned_attempts", {"self._tasks": sc([task(run_process_generation="pg-test")])}, binding=B)
add("_inspect_abandoned_attempts", {"self._tasks": sc([task()]), "self._transport_for": sc(tp()),
                                    "turn_lock": sc({}), "self._resume_exact": sc("s1"),
                                    "self._terminal_from_history": sc(receipt()),
                                    "transport.info": sc({"active": False}),
                                    "self._report_pending_action": sc(None), "state.settle_task": sc(None)},
    binding=B)
add("_inspect_abandoned_attempts", {"self._tasks": sc([task()]), "self._transport_for": sc(tp()),
                                    "turn_lock": sc({}), "self._resume_exact": sc("s1"),
                                    "self._terminal_from_history": sc(None),
                                    "transport.info": sc({"active": True, "task_id": "t1"}),
                                    "self._report_pending_action": sc(None)}, binding=B)
add("_inspect_abandoned_attempts", {"self._tasks": sc([task()]), "self._transport_for": sc(tp("rpc")),
                                    "turn_lock": sc({}), "transport.resolve_exact": sc({"session_id": "s1"}),
                                    "transport.info": sc({"active": False}),
                                    "self._report_pending_action": sc(None)}, binding=B)
add("_inspect_abandoned_attempts", {"self._tasks": sc([task()]),
                                    "self._transport_for": sc(rs("RuntimeError", "x"))}, binding=B)
add("_inspect_abandoned_attempts", {}, binding=B)   # fail-closed

# ── _harvest_previous_attempt ────────────────────────────────────────────────
add("_harvest_previous_attempt", {"state.settle_task": sc(None)}, binding=B, task=T, receipt=receipt())
add("_harvest_previous_attempt", {"state.settle_task": sc(rs("StaleLeaseError", "x"))},
    binding=B, task=T, receipt=receipt())
add("_harvest_previous_attempt", {"state.settle_task": sc(rs("StaleTaskError", "x"))},
    binding=B, task=T, receipt=receipt())
add("_harvest_previous_attempt", {"state.settle_task": sc(rs("RuntimeError", "x"))},
    binding=B, task=T, receipt=receipt())
add("_harvest_previous_attempt", {"state.settle_task": sc(rs("DriverStateError", "x"))},
    binding=B, task=T, receipt=receipt())
add("_harvest_previous_attempt", {}, binding=B, task=T, receipt=receipt())   # fail-closed


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-func count check (>=5)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(counts), OUT))
    if short:
        print("WARNING fewer than 5 cases:", short)
    else:
        print("all funcs have >=5 cases")


if __name__ == "__main__":
    main()