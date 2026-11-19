#!/usr/bin/env python3
"""Generate tests/fixtures/session_authority_flow.jsonl for the
session-authority FLOW oracle. >=5 cases per ported function (29 funcs).

Each case is {"func", "in": {"args", "state", "io"}}. io scripts the universal
seam (a plain value reused, a list consumed per call, or {"__script__": true,
"ret"/"raise"}). Claimed-sibling rets (admission_row / _receipt / _handle) are
computed to match the live dataclass asdict shapes. The oracle re-drives the
LIVE reference from "in"; the C harness drives the port from the same "in".
"""
import copy
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "session_authority_flow.jsonl")

CAPS = sorted(["session:read", "session:submit", "session:control",
               "session:approve", "session:respond", "session:operator"])


def actor(subject="u1", caps=None, tid="t1", pid="p1"):
    return {"subject": subject, "profile_id": pid,
            "capabilities": sorted(caps if caps is not None else CAPS), "transport_id": tid}


def ref(sid="s1", pid="p1"):
    return {"profile_id": pid, "session_id": sid}


def R(seq, aid, status, outcome=None, sid="s1", payload=None, gen=None,
      owner_epoch=None, request_id="r1", principal_id="p1", intent="queue",
      queued_owner_epoch=None, transcript_mark=None):
    d = {"seq": seq, "admission_id": aid, "request_id": request_id,
         "principal_id": principal_id, "target_session_id": sid,
         "payload": payload if payload is not None else {"text": "hi"},
         "intent": intent, "status": status, "outcome": outcome,
         "owner_epoch": owner_epoch, "generation": gen}
    if transcript_mark is not None:
        d["transcript_mark"] = transcript_mark
        d["queued_owner_epoch"] = queued_owner_epoch
    return d


def AR(record):
    d = dict(record)
    d.pop("queued_owner_epoch", None)
    return d


def REC(row, epoch=7, pid="p1"):
    steering = row.get("outcome") == "steering"
    return {"admission_id": row["admission_id"],
            "ref": {"profile_id": pid, "session_id": row["target_session_id"]},
            "sequence": row["seq"],
            "status": "started" if steering else row["status"],
            "outcome": None if steering else row.get("outcome"),
            "authority_epoch": row.get("owner_epoch") or epoch,
            "execution_generation": row.get("generation")}


def H(sid="s1", state="idle", gen=0, rev=0, epoch=7, iid="i1", pid="p1"):
    return {"ref": ref(sid, pid), "instance_id": iid, "authority_epoch": epoch,
            "revision": rev, "execution_generation": gen, "execution_state": state}


def live(route="telegram:c1", platform="telegram", subs=None, settling=None,
         task=None, rescan=False, background_wait=None, execution=None, token=None,
         sid="s1"):
    return {"route": route,
            "source": {"platform": platform, "user_id": "u1", "chat_id": route.split(":")[-1]},
            "settling": settling or {}, "subscribers": subs if subs is not None else {"sub1": actor()},
            "task": task, "rescan": rescan, "background_wait": background_wait,
            "pause_notified": False, "event_stream": {"execution": execution or {}},
            "token": token or sid}


def base_state():
    return {
        "profile_id": "p1", "instance_id": "i1", "epoch": 7, "retiring": False,
        "runner": {"_draining": False, "_is_session_running": True, "_running_agents": {}},
        "store_scope": {"profile_dir": "/h/p1", "hermes_home_key": "hk1"},
        "sessions": {"s1": live(), "s2": live(route="telegram:c2", sid="s2",
                                              subs={"sub2": actor(tid="t2")})},
        "waiters": {}, "native_waiters": [], "cancel_obligations": [],
        "pending_steers": {}, "adopted": {}, "executing": {},
        "_classic_evict_at": {}, "_route_epochs": {}, "events": {"t1": "t1", "t2": "t2"},
        "event": None,
    }


def raise_spec(type_, message="", **kw):
    d = {"__script__": True, "raise": {"type": type_, "message": message}}
    d["raise"].update(kw)
    return d


CASES = []

# Seams whose LIVE return is a LIST and which are called once per flow step:
# a bare array script would be consumed as a per-call queue (one element per
# call), so wrap the list result one level deeper -> [[...]]. admission_row is
# EXCLUDED: it is genuinely called once per record (a true per-call queue).
LIST_SEAMS = {"store.admissions.list", "pending_rows", "queued_controls",
              "admission_media_references"}


def add(func, args, io=None, state=None, **state_over):
    st = base_state()
    if state is not None:
        st = state
    st.update(state_over)
    io = dict(io or {})
    for nm in LIST_SEAMS:
        if nm in io and isinstance(io[nm], list):
            io[nm] = [io[nm]]
    CASES.append({"func": func, "in": {"args": args, "state": st, "io": io}})


# ── route_epoch ──
add("route_epoch", {"route": "telegram:c1"}, _route_epochs={"telegram:c1": 3})
add("route_epoch", {"route": "absent"})
add("route_epoch", {"route": "telegram:c1"}, _route_epochs={"telegram:c1": 0})
add("route_epoch", {"route": ""}, _route_epochs={"": 5})
add("route_epoch", {"route": "r2"}, _route_epochs={"r1": 1, "r2": 9})

# ── fence_route ──
add("fence_route", {"route": "telegram:c1"})
add("fence_route", {"route": "telegram:c1"}, _route_epochs={"telegram:c1": 4})
add("fence_route", {"route": "r2"}, _route_epochs={"r1": 1})
add("fence_route", {"route": ""}, _route_epochs={"": 2})
add("fence_route", {"route": "telegram:c1"}, _route_epochs={"telegram:c1": 0, "x": 8})

# ── _log_drain_failure ──
add("_log_drain_failure", {"task": "task1"}, {"task.cancelled": True})
add("_log_drain_failure", {"task": "task1"}, {"task.cancelled": False, "task.exception": None})
add("_log_drain_failure", {"task": "task1"}, {"task.cancelled": False, "task.exception": "boom"})
add("_log_drain_failure", {"task": "task1"}, {"task.cancelled": raise_spec("SystemExit", "x")})
add("_log_drain_failure", {"task": "task1"}, {})
add("_log_drain_failure", {"task": "task1"},
    {"task.cancelled": False, "task.exception": raise_spec("RuntimeError", "e")})

# ── _run_deferred_reset ──
add("_run_deferred_reset", {"route": "r1", "reset": False})
add("_run_deferred_reset", {"route": "r1", "reset": True}, {"reset": None})
add("_run_deferred_reset", {"route": "r1", "reset": True}, {"reset": raise_spec("RuntimeError", "x")})
add("_run_deferred_reset", {"route": "r1", "reset": True}, {"reset": raise_spec("SystemExit", "x")})
add("_run_deferred_reset", {"route": "r1", "reset": True}, {})
add("_run_deferred_reset", {"route": "r1", "reset": True}, {"reset": raise_spec("ValueError", "v")})

# ── backfill_legacy_session_owners ──
add("backfill_legacy_session_owners", {}, {"store.sessions.store_owner": None})
add("backfill_legacy_session_owners", {},
    {"store.sessions.store_owner": "p1", "store.sessions.backfill_session_owners": 5})
add("backfill_legacy_session_owners", {},
    {"store.sessions.store_owner": "p1", "store.sessions.backfill_session_owners": 0})
add("backfill_legacy_session_owners", {},
    {"store.sessions.store_owner": "p1",
     "store.sessions.backfill_session_owners": raise_spec("RuntimeError", "x"),
     "store_location": "/h/p1"})
add("backfill_legacy_session_owners", {},
    {"store.sessions.store_owner": "p1",
     "store.sessions.backfill_session_owners": raise_spec("SystemExit", "x")})
add("backfill_legacy_session_owners", {}, {})

# ── backfill_compression_created_sources ──
add("backfill_compression_created_sources", {},
    {"store.lineage.stamp_legacy_compression_chains": 3})
add("backfill_compression_created_sources", {},
    {"store.lineage.stamp_legacy_compression_chains": 0})
add("backfill_compression_created_sources", {},
    {"store.lineage.stamp_legacy_compression_chains": raise_spec("RuntimeError", "x"),
     "store_location": "/h/p1"})
add("backfill_compression_created_sources", {},
    {"store.lineage.stamp_legacy_compression_chains": raise_spec("SystemExit", "x")})
add("backfill_compression_created_sources", {}, {})
add("backfill_compression_created_sources", {},
    {"store.lineage.stamp_legacy_compression_chains": 12})

# ── check_approval_generation ──
add("check_approval_generation", {"session_id": "s1", "generation": 4},
    {"self._handle": H(gen=4, state="running")})
add("check_approval_generation", {"session_id": "s1", "generation": 4},
    {"self._handle": H(gen=5, state="running")})
add("check_approval_generation", {"session_id": "s1", "generation": 4},
    {"self._handle": H(gen=4, state="idle")})
add("check_approval_generation", {"session_id": "s1", "generation": 4},
    {"self._handle": raise_spec("AuthorityRefusal", "not_found", reason="not_found")})
add("check_approval_generation", {"session_id": "s1", "generation": 4}, {})
add("check_approval_generation", {"session_id": "s1", "generation": 4},
    {"self._handle": H(gen=None, state="running")})

# ── _drain_stopping ──
add("_drain_stopping", {"ref": ref(), "live_token": "s1"})
add("_drain_stopping", {"ref": ref(), "live_token": "s1"},
    {"store.admissions.list": [R(1, "a1", "queued")], "waiter.set_exception": None},
    runner={"_draining": True, "_is_session_running": True, "_running_agents": {}},
    waiters={"a1": {"done": False}}, native_waiters=["a1"])
add("_drain_stopping", {"ref": ref(), "live_token": "s1"},
    {"store.admissions.list": [], "waiter.set_exception": None}, retiring=True)
add("_drain_stopping", {"ref": ref(), "live_token": "OTHER"},
    {"store.admissions.list": []})
add("_drain_stopping", {"ref": ref("gone"), "live_token": "gone"},
    {"store.admissions.list": []})
add("_drain_stopping", {"ref": ref(), "live_token": "s1"},
    {"store.admissions.list": raise_spec("SystemExit", "x")},
    runner={"_draining": True, "_is_session_running": True, "_running_agents": {}})

# ── _pending_rows ──
add("_pending_rows", {"ref": ref()}, {"store.admissions.list": [], "admission_row": []})
r1, r2 = R(1, "a1", "queued"), R(2, "a2", "started", gen=4)
add("_pending_rows", {"ref": ref()},
    {"store.admissions.list": [r1, r2], "admission_row": [AR(r1), AR(r2)]})
rm = R(3, "a3", "started", gen=9)
add("_pending_rows", {"ref": ref()},
    {"store.admissions.list": [r1, r2], "admission_row": [AR(r1), AR(r2)]},
    sessions={"s1": live(settling={"a3": AR(rm)})})
add("_pending_rows", {"ref": ref()},
    {"store.admissions.list": raise_spec("RuntimeError", "x")})
add("_pending_rows", {"ref": ref()}, {})
add("_pending_rows", {"ref": ref()},
    {"store.admissions.list": [r1], "admission_row": [raise_spec("ValueError", "v")]})

# ── _retire_overflowed ──
add("_retire_overflowed", {"session_id": "s1", "transport": "t1"})
add("_retire_overflowed", {"session_id": "s1", "transport": "tX"})
add("_retire_overflowed", {"session_id": "s1", "transport": "t1"},
    sessions={"s1": live(subs={"sub1": actor(tid="t1"), "sub2": actor(tid="t2"),
                               "sub3": actor(tid="t1")})})
add("_retire_overflowed", {"session_id": "gone", "transport": "t1"})
add("_retire_overflowed", {"session_id": "s1", "transport": "t1"},
    sessions={"s1": live(subs={})})
add("_retire_overflowed", {"session_id": "s2", "transport": "t2"})

# ── unless_deleted ──
add("unless_deleted", {"ref": ref()}, {"read": {"ok": True}})
add("unless_deleted", {"ref": ref()},
    {"read": raise_spec("AuthorityRefusal", "stale_generation", reason="stale_generation"),
     "store.sessions.get": None})
add("unless_deleted", {"ref": ref()},
    {"read": raise_spec("AuthorityRefusal", "stale_generation", reason="stale_generation"),
     "store.sessions.get": {"session_id": "s1"}})
add("unless_deleted", {"ref": ref()}, {"read": raise_spec("RuntimeError", "x")})
add("unless_deleted", {"ref": ref()}, {})
add("unless_deleted", {"ref": ref()},
    {"read": raise_spec("AuthorityRefusal", "x", reason="x"),
     "store.sessions.get": raise_spec("SystemExit", "s")})

# ── resolve ──
add("resolve", {"actor": actor(), "ref": ref()},
    {"self.authorize": None, "self._handle": H(state="running", gen=4)})
add("resolve", {"actor": actor(), "ref": ref()},
    {"self.authorize": raise_spec("AuthorityRefusal", "permission_denied", reason="permission_denied")})
add("resolve", {"actor": actor(), "ref": ref()},
    {"self.authorize": raise_spec("SystemExit", "x")})
add("resolve", {"actor": actor(), "ref": ref()},
    {"self.authorize": None, "self._handle": raise_spec("RuntimeError", "x")})
add("resolve", {"actor": actor(), "ref": ref()}, {})
add("resolve", {"actor": actor(), "ref": ref()},
    {"self.authorize": None, "self._handle": H(state="idle")})

# ── _pending_receipt ──
pr_row = R(1, "a1", "queued", payload={"text": "hello"})
add("_pending_receipt", {"row": pr_row},
    {"queued_controls": ["steer"], "self._receipt": REC(pr_row)})
pr_row2 = R(2, "a2", "queued", payload={"native_text_v1": {"event": {"text": "native"}}})
add("_pending_receipt", {"row": pr_row2},
    {"queued_controls": [], "self._receipt": REC(pr_row2)})
pr_row3 = R(3, "a3", "queued", payload={})
add("_pending_receipt", {"row": pr_row3},
    {"queued_controls": [], "self._receipt": REC(pr_row3)})
pr_row4 = R(4, "a4", "started", outcome="steering", gen=4, payload={"text": "s"})
add("_pending_receipt", {"row": pr_row4},
    {"queued_controls": ["replace"], "self._receipt": REC(pr_row4)})
add("_pending_receipt", {"row": pr_row},
    {"queued_controls": raise_spec("RuntimeError", "x")})
add("_pending_receipt", {"row": pr_row}, {})

# ── _publish_pending ──
pp_io = {"restore_retired": None, "self._handle": H(state="running", gen=4, rev=2),
         "pending_rows": [], "event_stream.publish": True, "wake_bot_receipts": None}
add("_publish_pending", {"ref": ref()}, pp_io)
pp_row = R(1, "a1", "queued", payload={"text": "hi"})
pp_io2 = dict(pp_io, pending_rows=[pp_row], queued_controls=["steer"],
              **{"self._receipt": REC(pp_row)})
add("_publish_pending", {"ref": ref()}, pp_io2)
add("_publish_pending", {"ref": ref("gone")}, {"restore_retired": None})
add("_publish_pending", {"ref": ref()}, {"restore_retired": raise_spec("SystemExit", "x")})
add("_publish_pending", {"ref": ref()},
    dict(pp_io, **{"event_stream.publish": raise_spec("RuntimeError", "x")}))
add("_publish_pending", {"ref": ref()}, {})

# ── _schedule ──
add("_schedule", {"ref": ref()}, {"restore_retired": None, "asyncio.create_task": "task9"})
add("_schedule", {"ref": ref()}, {"restore_retired": None, "asyncio.create_task": "task9"},
    sessions={"s1": live(task={"id": "old", "done": True})})
add("_schedule", {"ref": ref()}, {"restore_retired": None},
    sessions={"s1": live(task={"id": "old", "done": False})})
add("_schedule", {"ref": ref()},
    {"restore_retired": None, "wake_background_wait": None, "asyncio.create_task": "task9"},
    sessions={"s1": live(background_wait=True)})
add("_schedule", {"ref": ref("gone")}, {"restore_retired": None})
add("_schedule", {"ref": ref()}, {"restore_retired": raise_spec("SystemExit", "x")})

# ── wake_after_worker ──
add("wake_after_worker", {"session_id": "s1"},
    {"self.logical_owner": "s1", "restore_retired": None, "asyncio.create_task": "task9"})
add("wake_after_worker", {"session_id": "s1"},
    {"self.logical_owner": "s2", "restore_retired": None, "asyncio.create_task": "task9"})
add("wake_after_worker", {"session_id": "s1"},
    {"self.logical_owner": "sX", "restore_retired": None, "asyncio.create_task": "task9"})
add("wake_after_worker", {"session_id": "gone"}, {"self.logical_owner": "gone"})
add("wake_after_worker", {"session_id": "s1"},
    {"self.logical_owner": raise_spec("RuntimeError", "x")})
add("wake_after_worker", {"session_id": "s1"}, {})

# ── _pause ──
add("_pause", {"ref": ref(), "reason": "session_busy"},
    {"store.admissions.list": [R(1, "a1", "queued")], "waiter.set_exception": None},
    waiters={"a1": {"done": False}}, native_waiters=["a1"])
add("_pause", {"ref": ref(), "reason": "session_busy"},
    {"store.admissions.list": [R(1, "a1", "queued")]},
    waiters={"a1": {"done": False}}, native_waiters=[])
add("_pause", {"ref": ref(), "reason": "runtime_draining"},
    {"store.admissions.list": [R(1, "a1", "queued"), R(2, "a2", "queued")],
     "waiter.set_exception": None},
    waiters={"a1": {"done": False}, "a2": {"done": False}}, native_waiters=["a1"])
add("_pause", {"ref": ref(), "reason": "runtime_draining"},
    {"store.admissions.list": [R(1, "a1", "queued")]},
    waiters={"a1": {"done": True}}, native_waiters=["a1"])
add("_pause", {"ref": ref(), "reason": "runtime_draining"},
    {"store.admissions.list": []})
add("_pause", {"ref": ref(), "reason": "runtime_draining"},
    {"store.admissions.list": raise_spec("SystemExit", "x")})

# ── _admitted ──
ad_row = R(1, "a1", "queued", payload={"text": "hi"})
ad_io = {"mark_producer_admission": None, "restore_retired": None,
         "self._handle": H(state="running", gen=4), "pending_rows": [],
         "event_stream.publish": True, "wake_bot_receipts": None,
         "self._receipt": REC(ad_row)}
add("_admitted", {"ref": ref(), "event": False, "row": ad_row}, ad_io)
add("_admitted", {"ref": ref(), "event": True, "row": ad_row},
    dict(ad_io, **{"event._on_commit": None}), event={"_on_commit": True})
ad_row_started = R(2, "a2", "started", gen=4, payload={"text": "hi"})
add("_admitted", {"ref": ref(), "event": True, "row": ad_row_started},
    dict(ad_io, **{"self._receipt": REC(ad_row_started)}), event={})
add("_admitted", {"ref": ref(), "event": True, "row": ad_row}, ad_io, event={})
add("_admitted", {"ref": ref(), "event": True, "row": ad_row},
    dict(ad_io, mark_producer_admission=raise_spec("SystemExit", "x")), event={})
add("_admitted", {"ref": ref(), "event": False, "row": ad_row}, {})

# ── _schedule_admitted ──
add("_schedule_admitted", {"ref": ref()}, {"restore_retired": None, "asyncio.create_task": "task9"})
add("_schedule_admitted", {"ref": ref()}, {"restore_retired": None},
    sessions={"s1": live(task={"id": "old", "done": False})})
add("_schedule_admitted", {"ref": ref()},
    {"restore_retired": None, "wake_background_wait": None, "asyncio.create_task": "task9"},
    sessions={"s1": live(background_wait=True)})
add("_schedule_admitted", {"ref": ref("gone")}, {"restore_retired": None})
add("_schedule_admitted", {"ref": ref()}, {"restore_retired": raise_spec("SystemExit", "x")})
add("_schedule_admitted", {"ref": ref()}, {})

# ── _release_refused_capture ──
rc_payload = {"attachments_v1": {"media": [{"id": "m1"}]}}
rc_io = {"drop_capture_lease": None, "admission_media_references": ["ref1"],
         "store.admissions.record_retired_media": None, "collect_retired_media": None}
add("_release_refused_capture", {"request": {"request_id": "r1"}, "payload": {"text": "hi"}})
add("_release_refused_capture", {"request": {"request_id": "r1"}, "payload": rc_payload}, rc_io)
add("_release_refused_capture", {"request": {"request_id": "r1"}, "payload": rc_payload},
    dict(rc_io, admission_media_references=raise_spec("RuntimeError", "x")))
add("_release_refused_capture", {"request": {"request_id": "r1"}, "payload": rc_payload},
    dict(rc_io, **{"store.admissions.record_retired_media": raise_spec("ValueError", "v")}))
add("_release_refused_capture", {"request": {"request_id": "r1"}, "payload": rc_payload},
    dict(rc_io, collect_retired_media=raise_spec("RuntimeError", "x")))
add("_release_refused_capture", {"request": {"request_id": "r1"}, "payload": rc_payload},
    dict(rc_io, drop_capture_lease=raise_spec("SystemExit", "x")))

# ── _commit_cancel ──
cc_before = REC(R(1, "a1", "queued"))
cc_rec = R(1, "a1", "terminal", outcome="cancelled")
cc_settle_io = {"store.admissions.cancel": cc_rec, "admission_row": AR(cc_rec),
                "restore_retired": None, "self._handle": H(), "pending_rows": [],
                "event_stream.publish": True, "wake_bot_receipts": None,
                "drop_producer_turn": None, "waiter.set_result": None,
                "asyncio.create_task": "task9", "busy_row_settled": None}
add("_commit_cancel", {"ref": ref(), "admission_id": "a1", "before": cc_before},
    cc_settle_io, cancel_obligations=[], waiters={"a1": {"done": False}}, native_waiters=["a1"])
cc_before_started = REC(R(1, "a1", "started", gen=4))
add("_commit_cancel", {"ref": ref(), "admission_id": "a1", "before": cc_before_started},
    {"store.admissions.cancel": cc_rec, "admission_row": AR(cc_rec),
     "restore_retired": None, "self._handle": H(), "pending_rows": [],
     "event_stream.publish": True, "wake_bot_receipts": None})
add("_commit_cancel", {"ref": ref(), "admission_id": "a1", "before": cc_before},
    {"store.admissions.cancel": raise_spec("AuthorityRefusal", "stale_generation",
                                          reason="stale_generation")},
    cancel_obligations=["a1"])
cc_q = R(1, "a1", "queued")
add("_commit_cancel", {"ref": ref(), "admission_id": "a1", "before": cc_before_started},
    {"store.admissions.cancel": cc_q, "admission_row": AR(cc_q),
     "restore_retired": None, "self._handle": H(), "pending_rows": [],
     "event_stream.publish": True, "wake_bot_receipts": None}, cancel_obligations=["a1"])
add("_commit_cancel", {"ref": ref(), "admission_id": "a1", "before": cc_before},
    {"store.admissions.cancel": cc_rec, "admission_row": raise_spec("RuntimeError", "x")})
add("_commit_cancel", {"ref": ref(), "admission_id": "a1", "before": cc_before}, {})

# ── _settle_cancelled ──
sc_io = {"restore_retired": None, "self._handle": H(), "pending_rows": [],
         "event_stream.publish": True, "wake_bot_receipts": None,
         "drop_producer_turn": None, "waiter.set_result": None,
         "asyncio.create_task": "task9", "busy_row_settled": None}
add("_settle_cancelled", {"ref": ref(), "admission_id": "a1"}, sc_io,
    waiters={"a1": {"done": False}}, native_waiters=["a1"], cancel_obligations=["a1"])
add("_settle_cancelled", {"ref": ref(), "admission_id": "a1"}, sc_io,
    waiters={"a1": {"done": True}}, native_waiters=["a1"], cancel_obligations=["a1"])
add("_settle_cancelled", {"ref": ref(), "admission_id": "a1"}, sc_io,
    waiters={}, native_waiters=[], cancel_obligations=[])
add("_settle_cancelled", {"ref": ref(), "admission_id": "a1"},
    dict(sc_io, **{"event_stream.publish": [True, raise_spec("RuntimeError", "x")]}),
    waiters={"a1": {"done": False}})
add("_settle_cancelled", {"ref": ref("gone"), "admission_id": "a1"}, {})
add("_settle_cancelled", {"ref": ref(), "admission_id": "a1", "outcome": "interrupted"}, sc_io,
    waiters={"a1": {"done": False}})

# ── _classic_turn_holds ──
add("_classic_turn_holds", {"route": "r1"},
    runner={"_draining": False, "_running_agents": {}})
add("_classic_turn_holds", {"route": "r1"}, {"runner._is_session_running": False})
add("_classic_turn_holds", {"route": "r1"},
    {"runner._is_session_running": [True, True, True], "time.monotonic": 100.0,
     "runner._hm_evict_idle_stale_agent": None, "runner._hm_evict_reaped_agent": None},
    _classic_evict_at={"r1": 0.0})
add("_classic_turn_holds", {"route": "r1"},
    {"runner._is_session_running": [True, True], "time.monotonic": 100.0},
    _classic_evict_at={"r1": 99.5})
add("_classic_turn_holds", {"route": "r1"},
    {"runner._is_session_running": [True, False, False], "time.monotonic": 100.0,
     "runner._hm_evict_idle_stale_agent": None}, _classic_evict_at={"r1": 0.0})
add("_classic_turn_holds", {"route": "r1"},
    {"runner._is_session_running": raise_spec("SystemExit", "x")})

# ── _runs_own_agent ──
add("_runs_own_agent", {"ref": ref()}, {"running_key": "telegram:c1"},
    runner={"_draining": False, "_is_session_running": True,
            "_running_agents": {"telegram:c1": "agent1"}})
add("_runs_own_agent", {"ref": ref()}, {"running_key": "telegram:c1"},
    runner={"_draining": False, "_is_session_running": True,
            "_running_agents": {"telegram:c1": "__sentinel__"}})
add("_runs_own_agent", {"ref": ref()}, {"running_key": "telegram:c1"},
    runner={"_draining": False, "_is_session_running": True, "_running_agents": {}})
add("_runs_own_agent", {"ref": ref()}, {"running_key": "other"},
    runner={"_draining": False, "_is_session_running": True,
            "_running_agents": {"telegram:c1": "agent1"}})
add("_runs_own_agent", {"ref": ref("gone")}, {})
add("_runs_own_agent", {"ref": ref()}, {"running_key": raise_spec("SystemExit", "x")})

# ── steer_agent ──
st_io = {"wake_background_wait": None}
add("steer_agent", {"ref": ref(), "generation": 4, "text": "go"},
    dict(st_io), adopted={"s1": [4, "agent1"]})
add("steer_agent", {"ref": ref(), "generation": 4, "text": "go"},
    dict(st_io, running_key="telegram:c1", **{"self.agent": "agent2"}),
    adopted={"s1": [3, "old"]},
    runner={"_draining": False, "_is_session_running": True,
            "_running_agents": {"telegram:c1": "agent2"}})
add("steer_agent", {"ref": ref(), "generation": 4, "text": None},
    dict(st_io, running_key="telegram:c1"), adopted={"s1": [3, "old"]},
    runner={"_draining": False, "_is_session_running": True, "_running_agents": {}})
add("steer_agent", {"ref": ref(), "generation": 4, "text": "go"},
    dict(st_io, running_key="telegram:c1", adopts_owner_agent=True),
    adopted={"s1": [3, "old"]},
    runner={"_draining": False, "_is_session_running": True, "_running_agents": {}},
    pending_steers={"s1": [4, ["prev"]]})
add("steer_agent", {"ref": ref(), "generation": 4, "text": "go"},
    dict(st_io, running_key="telegram:c1", adopts_owner_agent=False),
    adopted={"s1": [3, "old"]},
    runner={"_draining": False, "_is_session_running": True, "_running_agents": {}})
add("steer_agent", {"ref": ref(), "generation": 4, "text": "go"},
    {"wake_background_wait": raise_spec("SystemExit", "x")})

# ── register_approval ──
add("register_approval", {"session_id": "s1", "generation": 4, "route": "r1", "data": {"x": 1}},
    {"self._handle": H(gen=4, state="running"), "controls.register": None})
add("register_approval", {"session_id": "s1", "generation": 4, "route": "r1", "data": {}},
    {"self._handle": H(gen=5, state="running")})
add("register_approval", {"session_id": "gone", "generation": 4, "route": "r1", "data": {}}, {})
add("register_approval", {"session_id": "s1", "generation": 4, "route": "r1", "data": {}},
    {"self._handle": H(gen=4, state="running"), "controls.register": raise_spec("SystemExit", "x")})
add("register_approval", {"session_id": "s1", "generation": 4, "route": "r1", "data": {}}, {})
add("register_approval", {"session_id": "s1", "generation": 4, "route": "r1", "data": {},
                          "timeout_s": 30},
    {"self._handle": H(gen=4, state="running"), "controls.register": None})

# ── register_clarify ──
add("register_clarify", {"session_id": "s1", "generation": 4, "entry": {"q": "?"}},
    {"self._handle": H(gen=4, state="running"), "controls.register_clarify": None})
add("register_clarify", {"session_id": "s1", "generation": 4, "entry": {}},
    {"self._handle": H(gen=4, state="idle")})
add("register_clarify", {"session_id": "gone", "generation": 4, "entry": {}}, {})
add("register_clarify", {"session_id": "s1", "generation": 4, "entry": {}},
    {"self._handle": H(gen=4, state="running"),
     "controls.register_clarify": raise_spec("SystemExit", "x")})
add("register_clarify", {"session_id": "s1", "generation": 4, "entry": {}}, {})
add("register_clarify", {"session_id": "s1", "generation": 4, "entry": {}, "timeout_s": 10},
    {"self._handle": H(gen=4, state="running"), "controls.register_clarify": None})

# ── respond ──
rp_io = {"self.authorize": None, "self._handle": H(gen=4, state="running"),
         "controls.respond": {"answered": True}}
add("respond", {"actor": actor(), "ref": ref(), "generation": 4, "prompt_id": "p1",
                "response": {"v": 1}}, rp_io)
add("respond", {"actor": actor(), "ref": ref(), "generation": 4, "prompt_id": "p1",
                "response": {}, "kind": "clarify"}, rp_io)
add("respond", {"actor": actor(), "ref": ref(), "generation": 4, "prompt_id": "p1",
                "response": {}, "kind": "bogus"}, {})
add("respond", {"actor": actor(subject="other"), "ref": ref(), "generation": 4,
                "prompt_id": "p1", "response": {}}, {"self.authorize": None})
add("respond", {"actor": actor(), "ref": ref(), "generation": True, "prompt_id": "p1",
                "response": {}}, {"self.authorize": None})
add("respond", {"actor": actor(), "ref": ref(), "generation": 4, "prompt_id": "",
                "response": {}}, {"self.authorize": None, "self._handle": H(gen=4, state="running")})

# ── _stamp_claim ──
sc_row = R(1, "a1", "started", gen=4, payload={"text": "hi"}, transcript_mark=10)
scp_io = {"admission_display": {"display": "hi"}, "event_stream.publish": True,
          "restore_retired": None, "self._handle": H(state="running", gen=4),
          "pending_rows": [], "wake_bot_receipts": None}
add("_stamp_claim", {"ref": ref(), "row": AR(sc_row)}, scp_io)
add("_stamp_claim", {"ref": ref(), "row": None}, {})
add("_stamp_claim", {"ref": ref(), "row": False}, {})
add("_stamp_claim", {"ref": ref(), "row": AR(sc_row)},
    dict(scp_io, **{"event_stream.publish": raise_spec("RuntimeError", "x")}))
add("_stamp_claim", {"ref": ref("gone"), "row": AR(sc_row)}, {})
add("_stamp_claim", {"ref": ref(), "row": AR(sc_row)},
    {"admission_display": {"display": "hi"}})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for case in CASES:
        counts[case["func"]] = counts.get(case["func"], 0) + 1
    short = {k: v for k, v in counts.items() if v < 5}
    print(f"wrote {len(CASES)} cases across {len(counts)} funcs -> {OUT}")
    if short:
        print(f"WARNING funcs with <5 cases: {short}")
        raise SystemExit(1)


if __name__ == "__main__":
    main()