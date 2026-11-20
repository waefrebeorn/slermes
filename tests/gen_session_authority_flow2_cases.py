#!/usr/bin/env python3
"""Generate tests/fixtures/session_authority_flow2.jsonl for the
session-authority FLOW remainder oracle. >=5 cases per ported function (19).

Each case is {"func", "in": {"args", "state", "io"}}. io scripts the universal
seam (a plain value reused, a list consumed per call, or {"__script__": true,
"ret"/"raise"}). Claimed-sibling rets (admission_row / _receipt / _handle) are
computed to match the live dataclass asdict shapes. The oracle re-drives the
LIVE reference from "in"; the C harness drives the port from the same "in".
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "session_authority_flow2.jsonl")
CAPS = sorted(["session:read", "session:submit", "session:control",
               "session:approve", "session:respond", "session:operator"])


def actor(subject="u1", caps=None, tid="t1", pid="p1"):
    return {"subject": subject, "profile_id": pid,
            "capabilities": sorted(caps if caps is not None else CAPS), "transport_id": tid}


def ref(sid="s1", pid="p1"):
    return {"profile_id": pid, "session_id": sid}


def src(platform="telegram", chat="c1", user="u1"):
    return {"platform": platform, "user_id": user, "chat_id": chat}


def R(seq, aid, status, outcome=None, sid="s1", payload=None, gen=None,
      owner_epoch=None, request_id="r1", principal_id="u1", intent="queue"):
    return {"seq": seq, "admission_id": aid, "request_id": request_id,
            "principal_id": principal_id, "target_session_id": sid,
            "payload": payload if payload is not None else {"text": "hi"},
            "intent": intent, "status": status, "outcome": outcome,
            "owner_epoch": owner_epoch, "generation": gen}


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
         sid="s1", source=None):
    return {"route": route,
            "source": source if source is not None else src(platform, route.split(":")[-1]),
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
        "pending_results": {}, "pending_stops": {}, "delivered_stops": {},
    }


def raise_spec(type_, message="", **kw):
    d = {"__script__": True, "raise": {"type": type_, "message": message}}
    d["raise"].update(kw)
    return d


def refusal(reason, **kw):
    return raise_spec("AuthorityRefusal", reason, reason=reason, **kw)


CASES = []
# Scripting convention (the universal seam reads a bare array as a PER-CALL
# queue, one element per call): a seam whose live result is itself a LIST or a
# TUPLE must be scripted one level deeper — [[...]] for a single list/tuple
# result, [[r1],[r2]] for two calls. admission_row is a genuine per-record queue
# (bare [AR1,AR2,...]); scalar/record seams reuse a plain value or queue.
LIST_SEAMS = set()


def add(func, args, io=None, state=None, **state_over):
    st = base_state() if state is None else state
    st.update(state_over)
    io = dict(io or {})
    for nm in LIST_SEAMS:
        if nm in io and isinstance(io[nm], list):
            io[nm] = [io[nm]]
    CASES.append({"func": func, "in": {"args": args, "state": st, "io": io}})


# ── detached ──
add("detached", {"session_id": "s1"}, {"detached_route_of": None})
add("detached", {"session_id": "s1"}, {"detached_route_of": "telegram:c1"})
add("detached", {"session_id": "s1"}, {"detached_route_of": None},
    sessions={"s1": live(source=None)})
add("detached", {"session_id": "absent"}, {})
add("detached", {"session_id": "s1"}, {"detached_route_of": raise_spec("RuntimeError", "x")})
add("detached", {"session_id": "s1"}, {})

# ── _reserve_route ──
RR_IO = {"self._require_admission_open": None,
         "runner.session_store.get_or_create_session": {"session_id": "s1", "session_key": "telegram:c1"},
         "self.logical_owner": "s1", "store.sessions.get": {"session_id": "s1"}}
add("_reserve_route", {"source": src()}, dict(RR_IO))
add("_reserve_route", {"source": src()}, dict(RR_IO, **{"store.sessions.get": None,
                                                        "store.sessions.create": None}))
add("_reserve_route", {"source": src()}, dict(RR_IO, **{"self._require_admission_open": refusal("runtime_draining")}))
add("_reserve_route", {"source": src()}, dict(RR_IO, **{"runner.session_store.get_or_create_session": None}))
add("_reserve_route", {"source": src()}, dict(RR_IO, **{"self.logical_owner": raise_spec("RuntimeError", "x")}))
add("_reserve_route", {"source": src()}, {})
add("_reserve_route", {"source": src()}, dict(RR_IO, **{"self.logical_owner": "s2",
                                                        "store.sessions.get": None, "store.sessions.create": None}))

# ── name_lane ──
add("name_lane", {"route": "telegram:c1", "session_id": "s1", "source": src()}, {})
add("name_lane", {"route": "telegram:c1", "session_id": "s2", "source": src()},
    {"detach": None, "detached_route_of": None})
add("name_lane", {"route": "telegram:c1", "session_id": "s2", "source": src()},
    {"detach": None, "detached_route_of": "telegram:c2", "forget_detached": None})
add("name_lane", {"route": "telegram:c1", "session_id": "s2", "source": src()},
    {"detach": refusal("permission_denied")})
add("name_lane", {"route": "telegram:c1", "session_id": "s1", "source": src()},
    {"detached_route_of": raise_spec("RuntimeError", "x")},
    sessions={"s1": live(route="telegram:c9"), "s2": live(route="telegram:c2", sid="s2")})
add("name_lane", {"route": "telegram:c1", "session_id": "s2", "source": src()}, {})

# ── _publish_route ──
add("_publish_route", {"source": src(), "session_id": "s1", "route": "telegram:c1"},
    {"self._require_admission_open": None})
add("_publish_route", {"source": src(), "session_id": "s3", "route": "telegram:c3"},
    {"self._require_admission_open": None})
add("_publish_route", {"source": src(), "session_id": "s1", "route": "telegram:c1"},
    {"self._require_admission_open": refusal("runtime_draining")})
add("_publish_route", {"source": src(), "session_id": "s2", "route": "telegram:c1"},
    {"self._require_admission_open": None, "detach": None, "detached_route_of": None})
add("_publish_route", {"source": src(), "session_id": "s1", "route": "telegram:c1"}, {})
add("_publish_route", {"source": src(), "session_id": "s2", "route": "telegram:c1"},
    {"self._require_admission_open": None, "detach": raise_spec("RuntimeError", "x")})

# ── register ──
REG_IO = dict(RR_IO, **{"self._require_admission_open": None})
add("register", {"source": src()}, dict(REG_IO))
add("register", {"source": src()}, dict(REG_IO, **{"store.sessions.get": None, "store.sessions.create": None}))
add("register", {"source": src()}, dict(REG_IO, **{"self._require_admission_open": refusal("runtime_draining")}))
add("register", {"source": src()}, {})
add("register", {"source": src()}, dict(REG_IO, **{"self.logical_owner": "s2", "store.sessions.get": None,
                                                   "store.sessions.create": None}))

# ── agent ──
add("agent", {"ref": ref()}, {"running_key": "telegram:c1:s1", "runner._cached_agent_for": "ag1"})
add("agent", {"ref": ref()}, {"running_key": "telegram:c1:s1", "runner._resident_agent_for": "ag2"},
    runner={"_draining": False, "_resident_agent_for": True, "_running_agents": {}})
add("agent", {"ref": ref()}, {"running_key": "telegram:c1:s1", "runner._cached_agent_for": None})
add("agent", {"ref": ref()}, {"running_key": raise_spec("RuntimeError", "x")})
add("agent", {"ref": ref("absent")}, {})
add("agent", {"ref": ref()}, {})

# ── _admit_native_write ──
NW_PAYLOAD = {"text": "hi", "native_text_v1": {"media": ["m1"], "event": {"text": "hi", "message_id": "mid1"}}}
NW_ROW = R(1, "a1", "queued", payload=NW_PAYLOAD)
NW_IO = {"self._current_owner": "s1", "reconcile_native_retry": NW_PAYLOAD,
         "self.gate_refusal": None, "store.admissions.admit": [[NW_ROW, True]],
         "admission_row": AR(NW_ROW)}
NW_ARGS = {"principal_id": "messaging:x", "session_id": "s1", "request_id": "r1",
           "payload": NW_PAYLOAD, "fence": True, "rewrite": {"rewrite": "rw1", "applied": False}, "raw": None}
add("_admit_native_write", dict(NW_ARGS), dict(NW_IO))
add("_admit_native_write", dict(NW_ARGS, rewrite=None), dict(NW_IO))
add("_admit_native_write", dict(NW_ARGS), dict(NW_IO, **{"store.admissions.admit": refusal("admission_conflict"),
                                                         "drop_capture_lease": None, "release_unheld_media": None}))
add("_admit_native_write", dict(NW_ARGS), dict(NW_IO, **{"reconcile_native_retry": raise_spec("RuntimeError", "x"),
                                                         "drop_capture_lease": None, "release_unheld_media": None}))
add("_admit_native_write", dict(NW_ARGS), dict(NW_IO, **{"store.admissions.admit": raise_spec("SystemExit", "x")}))
add("_admit_native_write", dict(NW_ARGS), {})
add("_admit_native_write", dict(NW_ARGS, payload={"text": "hi"}),
    {"self._current_owner": "s1", "reconcile_native_retry": {"text": "hi"}, "self.gate_refusal": None,
     "store.admissions.admit": [[NW_ROW, False]], "admission_row": AR(NW_ROW)})

# ── receipt ──
RC_ROW = R(1, "a1", "queued", sid="s1", principal_id="u1")
RC_IO = {"self.authorize": None, "store.admissions.get": RC_ROW, "admission_row": AR(RC_ROW),
         "same_owner": True, "self._receipt": REC(RC_ROW)}
RC_ARGS = {"actor": actor(), "ref": ref(), "admission_id": "a1"}
add("receipt", dict(RC_ARGS), dict(RC_IO))
add("receipt", dict(RC_ARGS), dict(RC_IO, **{"store.admissions.get": None, "admission_row": None}))
add("receipt", dict(RC_ARGS), dict(RC_IO, **{"admission_row": AR(R(1, "a1", "queued", sid="other"))}))
add("receipt", dict(RC_ARGS, actor=actor(caps=["session:read"])),
    dict(RC_IO, **{"same_owner": False}))
add("receipt", dict(RC_ARGS, actor=actor(caps=["session:read", "session:control"])),
    dict(RC_IO, **{"same_owner": False}))
add("receipt", dict(RC_ARGS), dict(RC_IO, **{"self.authorize": refusal("not_found")}))
add("receipt", dict(RC_ARGS), {})
MASK = R(2, "a1", "started", sid="s1", gen=3)
add("receipt", dict(RC_ARGS), dict(RC_IO, **{"self._receipt": REC(MASK)}),
    sessions={"s1": live(settling={"a1": MASK})})

# ── cancel_queued ──
CQ_ROW = R(1, "a1", "terminal", "cancelled", sid="s1")
CQ_IO = dict(RC_IO, **{"self._commit_cancel": CQ_ROW, "release_admission_media": None,
                       "self._receipt": [REC(RC_ROW), REC(CQ_ROW)]})
add("cancel_queued", dict(RC_ARGS), dict(CQ_IO))
add("cancel_queued", dict(RC_ARGS), dict(CQ_IO, **{"self.authorize": refusal("not_found")}))
add("cancel_queued", dict(RC_ARGS), dict(CQ_IO, **{"self._commit_cancel": refusal("stale_generation")}))
add("cancel_queued", dict(RC_ARGS), dict(CQ_IO, **{"release_admission_media": raise_spec("RuntimeError", "x")}))
add("cancel_queued", dict(RC_ARGS), {})

# ── cancel_or_claimed ──
CC_STARTED = R(1, "a1", "started", sid="s1", gen=2)
add("cancel_or_claimed", dict(RC_ARGS), dict(CQ_IO))
add("cancel_or_claimed", dict(RC_ARGS),
    dict(RC_IO, **{"self._commit_cancel": refusal("stale_generation"),
                   "store.admissions.get": [RC_ROW, CC_STARTED], "admission_row": [AR(RC_ROW), AR(CC_STARTED)]}))
add("cancel_or_claimed", dict(RC_ARGS),
    dict(RC_IO, **{"self._commit_cancel": refusal("stale_generation"),
                   "store.admissions.get": [RC_ROW, RC_ROW], "admission_row": [AR(RC_ROW), AR(RC_ROW)]}))
add("cancel_or_claimed", dict(RC_ARGS),
    dict(RC_IO, **{"self._commit_cancel": refusal("stale_generation"),
                   "store.admissions.get": [RC_ROW, None], "admission_row": [AR(RC_ROW), None]}))
add("cancel_or_claimed", dict(RC_ARGS), dict(RC_IO, **{"self._commit_cancel": refusal("not_found")}))
add("cancel_or_claimed", dict(RC_ARGS), {})

# ── resolve_unknown_as_owner ──
RU_ROW = R(1, "a1", "unknown", sid="s1", gen=2, owner_epoch=7)
RU_REC = {"admission_id": "a1", "status": "unknown", "outcome": None}
RU_IO = {"store.admissions.get": RU_REC, "store.admissions.resolve_unknown": RU_REC,
         "admission_row": AR(RU_ROW), "self._publish_pending": None, "self._schedule": None,
         "self._receipt": REC(RU_ROW), "deliver_resolved": None, "release_media_after_delivery": None}
RU_ARGS = {"ref": ref(), "admission_id": "a1", "generation": 2, "notice": None}
add("resolve_unknown_as_owner", dict(RU_ARGS), dict(RU_IO))
CAP = {"result": {"final_response": "the answer"}, "usage": {}}
PREP = ["completed", {"result": {"final_response": "the answer"}}]
RU_IO2 = {"store.admissions.get": [RU_REC, RU_REC], "admission_row": AR(RU_ROW),
          "prepare_result": [PREP], "store.admissions.resolve_unknown": RU_REC,
          "self._publish_pending": None, "self._schedule": None, "self._receipt": REC(RU_ROW),
          "_publish_completion": None, "deliver_resolved": None, "release_media_after_delivery": None}
add("resolve_unknown_as_owner", dict(RU_ARGS), dict(RU_IO2), pending_results={"a1": CAP})
add("resolve_unknown_as_owner", dict(RU_ARGS), dict(RU_IO, **{"store.admissions.get": None}))
add("resolve_unknown_as_owner", dict(RU_ARGS), dict(RU_IO, **{"store.admissions.resolve_unknown": refusal("stale_generation")}))
add("resolve_unknown_as_owner", dict(RU_ARGS), {})
add("resolve_unknown_as_owner", dict(RU_ARGS, notice="interrupted"), dict(RU_IO))

# ── _resolved_unknown ──
RUU_ARGS = {"ref": ref(), "admission_id": "a1", "prepared": None, "captured": None, "record": RU_ROW}
RUU_IO = {"admission_row": AR(RU_ROW), "hand_collection_to_drain": None,
          "self._publish_pending": None, "self._schedule": None, "self._receipt": REC(RU_ROW)}
add("_resolved_unknown", dict(RUU_ARGS), dict(RUU_IO),
    sessions={"s1": live(execution={"admission_id": "a1"})})
add("_resolved_unknown", dict(RUU_ARGS),
    {"admission_row": AR(RU_ROW), "deliver_resolved": None, "release_media_after_delivery": None,
     "self._publish_pending": None, "self._schedule": None, "self._receipt": REC(RU_ROW)})
add("_resolved_unknown", dict(RUU_ARGS, prepared=PREP, captured=CAP),
    {"admission_row": AR(RU_ROW), "_publish_completion": None, "deliver_resolved": None,
     "release_media_after_delivery": None, "self._publish_pending": None, "self._schedule": None,
     "self._receipt": REC(RU_ROW)})
add("_resolved_unknown", dict(RUU_ARGS),
    {"admission_row": AR(RU_ROW), "restore_retired": None, "deliver_resolved": None,
     "release_media_after_delivery": None, "self._publish_pending": None, "self._schedule": None,
     "self._receipt": REC(RU_ROW)}, sessions={})
add("_resolved_unknown", dict(RUU_ARGS),
    {"admission_row": AR(RU_ROW), "deliver_resolved": raise_spec("RuntimeError", "x"),
     "release_media_after_delivery": None})
add("_resolved_unknown", dict(RUU_ARGS), {})

# ── resolve_unknown ──
RUK_IO = dict(RC_IO, **{"store.admissions.get": [RC_ROW, RU_REC],
                        "admission_row": [AR(RC_ROW), AR(RU_ROW)],
                        "store.admissions.resolve_unknown": RU_REC,
                        "self._publish_pending": None, "self._schedule": None,
                        "self._receipt": [REC(RC_ROW), REC(RU_ROW)],
                        "deliver_resolved": None, "release_media_after_delivery": None})
RUK_ARGS = {"actor": actor(), "ref": ref(), "admission_id": "a1", "generation": 2}
add("resolve_unknown", dict(RUK_ARGS), dict(RUK_IO))
add("resolve_unknown", dict(RUK_ARGS), dict(RUK_IO, **{"self.authorize": refusal("permission_denied")}))
add("resolve_unknown", dict(RUK_ARGS), dict(RC_IO, **{"store.admissions.get": None, "admission_row": None}))
add("resolve_unknown", dict(RUK_ARGS), {})
add("resolve_unknown", dict(RUK_ARGS), dict(RUK_IO, **{"store.admissions.resolve_unknown": refusal("stale_generation")}))

# ── interrupt ──
IT_ARGS = {"actor": actor(), "ref": ref(), "generation": 2}
IT_IO = {"self.authorize": None, "self._handle": [H(state="running", gen=2), H(state="running", gen=2)],
         "wake_background_wait": None, "request_hard_interrupt": None}
add("interrupt", dict(IT_ARGS), dict(IT_IO), adopted={"s1": [2, "ag1"]})
add("interrupt", dict(IT_ARGS),
    {"self.authorize": None, "self._handle": [H(state="running", gen=2), H(state="running", gen=2)],
     "wake_background_wait": None},
    adopted={"s1": [1, "ag1"]}, executing={"s1": 9},
    sessions={"s1": live(execution={"execution_generation": 2})})
add("interrupt", dict(IT_ARGS),
    {"self.authorize": None, "self._handle": [H(state="running", gen=2), H(state="running", gen=2)],
     "running_key": "telegram:c1:s1", "runner._cached_agent_for": "ag9",
     "self._runs_own_agent": False, "wake_background_wait": None, "request_hard_interrupt": None},
    adopted={"s1": [1, "ag1"]}, executing={"s1": 2})
add("interrupt", dict(IT_ARGS), {"self.authorize": None, "self._handle": H(state="running", gen=5)})
add("interrupt", dict(IT_ARGS),
    {"self.authorize": None, "self._handle": [H(state="idle", gen=0), H(state="idle", gen=0)],
     "wake_background_wait": None})
add("interrupt", dict(IT_ARGS), {"self.authorize": refusal("not_found")})
add("interrupt", dict(IT_ARGS), {})

# ── _refuse_head ──
RH_ARGS = {"ref": ref(), "admission_id": "a1", "reason": "admission_conflict"}
RH_REC = {"admission_id": "a1", "status": "queued", "outcome": None}
RH_TERM = {"admission_id": "a1", "status": "terminal", "outcome": "cancelled"}
RH_IO = {"store.admissions.get": RH_REC, "store.admissions.cancel": RH_TERM,
         "waiter.set_exception": None, "self._settle_cancelled": None, "release_admission_media": None}
add("_refuse_head", dict(RH_ARGS), dict(RH_IO), waiters={"a1": {"done": False}})
add("_refuse_head", dict(RH_ARGS), dict(RH_IO, **{"store.admissions.cancel": refusal("not_found")}))
add("_refuse_head", dict(RH_ARGS), dict(RH_IO, **{"store.admissions.cancel": RH_REC}))
add("_refuse_head", dict(RH_ARGS), dict(RH_IO, **{"store.admissions.get": None}))
add("_refuse_head", dict(RH_ARGS), dict(RH_IO, **{"store.admissions.cancel": raise_spec("RuntimeError", "x")}))
add("_refuse_head", dict(RH_ARGS), {})

# ── recover_native_sessions ──
NV_ROW = R(1, "a1", "queued", sid="s1", payload={"native_text_v1": {"event": {"text": "hi"}}})
RCV_IO = {"store.admissions.list": [[NV_ROW]], "admission_row": AR(NV_ROW),
          "self.physical_target": "s1", "check_native_route": [[src(), "telegram:c1"]],
          "self._require_admission_open": None, "self._schedule": None}
RCV_ARGS = {"bindings": [["s1", src(), "adapter1"]]}
add("recover_native_sessions", dict(RCV_ARGS), dict(RCV_IO))
add("recover_native_sessions", {"bindings": [["s1", src(), "a1"], ["s1", src(), "a2"]]}, {})
add("recover_native_sessions", dict(RCV_ARGS),
    {"store.admissions.list": [[R(1, "a1", "queued", payload={"text": "hi"})]],
     "admission_row": AR(R(1, "a1", "queued", payload={"text": "hi"}))})
add("recover_native_sessions", dict(RCV_ARGS),
    dict(RCV_IO, **{"store.admissions.list": [[R(1, "a1", "unknown", payload={"native_text_v1": {}})]],
                    "admission_row": AR(R(1, "a1", "unknown", payload={"native_text_v1": {}}))}))
add("recover_native_sessions", dict(RCV_ARGS), dict(RCV_IO, **{"check_native_route": refusal("admission_conflict")}))
add("recover_native_sessions", {"bindings": [["s1", src(), "a1"], ["s2", src("telegram", "c2"), "a2"]]},
    {"store.admissions.list": [[NV_ROW], [R(1, "a2", "queued", sid="s2", payload={"native_text_v1": {}})]],
     "admission_row": [AR(NV_ROW), AR(R(1, "a2", "queued", sid="s2", payload={"native_text_v1": {}}))],
     "self.physical_target": ["s1", "s2"], "check_native_route": [[src(), "telegram:c1"], [src("telegram", "c2"), "telegram:c2"]],
     "self._require_admission_open": None, "self._schedule": None})
add("recover_native_sessions", dict(RCV_ARGS), {})

# ── admit_automation ──
AA_ARGS = {"adapter": "adapter1", "event": {"text": "hi"}, "identity": {"user_id": "u1"},
           "replaces": None, "fold": None}
add("admit_automation", dict(AA_ARGS), {"admit_automation": {"ok": True}})
add("admit_automation", dict(AA_ARGS), {"admit_automation": None})
add("admit_automation", dict(AA_ARGS), {"admit_automation": refusal("runtime_draining")})
add("admit_automation", dict(AA_ARGS), {"admit_automation": raise_spec("SystemExit", "x")})
add("admit_automation", dict(AA_ARGS), {})
add("admit_automation", dict(AA_ARGS, replaces="a0", fold={"skip_ids": []}), {"admit_automation": {"ok": 1}})

# ── _submit ──
SUB_REQ = {"ref": ref(), "intent": "queue", "payload": {"text": "hi"}, "request_id": "r1"}
SUB_ROW = R(1, "a1", "queued", sid="s1")
SUB_IO = {"self._require_admission_open": None, "admit_finite": {}, "admit_surface": {},
          "admit_display": {}, "admit_cli_turn": {}, "admit_attachments": {},
          "same_owner": True, "self.gate_refusal": None, "store.admissions.admit": [[SUB_ROW, True]],
          "admission_row": AR(SUB_ROW), "self._admitted": REC(SUB_ROW), "self._schedule_admitted": None}
SUB_ARGS = {"actor": actor(), "request": SUB_REQ, "queue_only": False, "_kanban_card": None,
            "replaces": None, "supersedes": None, "api_target": None, "hosted": None}
add("_submit", dict(SUB_ARGS), dict(SUB_IO))
add("_submit", dict(SUB_ARGS, request=dict(SUB_REQ, intent="run")), {})
add("_submit", dict(SUB_ARGS, request=dict(SUB_REQ, payload={})), {})
add("_submit", dict(SUB_ARGS, request=dict(SUB_REQ, payload={"text": "hi", "bogus": 1})), {})
add("_submit", dict(SUB_ARGS, request=dict(SUB_REQ, payload={"text": 5})), {})
add("_submit", dict(SUB_ARGS), dict(SUB_IO, **{"same_owner": False}))
add("_submit", dict(SUB_ARGS), dict(SUB_IO, **{"store.admissions.admit": refusal("admission_conflict"),
                                              "self._release_refused_capture": None}))
add("_submit", dict(SUB_ARGS), dict(SUB_IO, **{"store.admissions.admit": raise_spec("SystemExit", "x")}))
add("_submit", dict(SUB_ARGS, _kanban_card={"card": "k1"}), dict(SUB_IO))
add("_submit", dict(SUB_ARGS), {})

# ── submit ──
SUBM_IO = dict(SUB_IO, **{"self.authorize": None, "capture_lease": None})
SUBM_ARGS = {"actor": actor(), "request": SUB_REQ, "queue_while_restarting": False,
             "_kanban_card": None, "replaces": None, "supersedes": None, "api_target": None, "hosted": None}
add("submit", dict(SUBM_ARGS), dict(SUBM_IO))
add("submit", dict(SUBM_ARGS), dict(SUBM_IO, **{"self.authorize": refusal("not_found")}))
add("submit", dict(SUBM_ARGS, request=dict(SUB_REQ, ref=ref("absent"))), {"self.authorize": None})
add("submit", dict(SUBM_ARGS, queue_while_restarting=True), dict(SUBM_IO))
add("submit", dict(SUBM_ARGS), {})
add("submit", dict(SUBM_ARGS), dict(SUBM_IO, **{"capture_lease": raise_spec("RuntimeError", "x")}))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for case in CASES:
        counts[case["func"]] = counts.get(case["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for fn in sorted(counts):
        flag = "" if counts[fn] >= 5 else "  (<5!)"
        print(f"  {fn}: {counts[fn]}{flag}")
    short = [fn for fn, n in counts.items() if n < 5]
    if short:
        raise SystemExit(f"functions with <5 cases: {short}")


if __name__ == "__main__":
    main()