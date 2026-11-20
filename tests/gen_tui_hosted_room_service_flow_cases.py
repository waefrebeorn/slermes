#!/usr/bin/env python3
"""Generate tests/fixtures/tui_hosted_room_service_flow.jsonl — scripted-seam
FLOW cases for the 20-def tui_hosted_room_service_flow port.

Every case: {"func": <harness name>, "in": {... inputs ..., "seams": {name: script}}}
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). {"__raise__": cls, "msg": m} raises that exception class in the live
drive and is delivered as a raise descriptor to the C universal seam. A MISSING
seam key fail-closes (RuntimeError) on BOTH sides.

Domains (documented in the port header): str() exercised on scalars only;
int() on number/bool/plain-digit-string/null; identity/exc/decision/binding/
service/self are attribute doubles; _events fixtures terminate.

Run: python tests/gen_tui_hosted_room_service_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "tui_hosted_room_service_flow.jsonl")
CASES = []


def add(func, inp, seams=None):
    inp = dict(inp)
    inp["seams"] = seams or {}
    CASES.append({"func": func, "in": inp})


def rs(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def task(member=None, profile=None, status="running", tid="t1", gen=2,
         started=None, room="r1", payload=True):
    d = {"identity": {"task_id": tid, "thread_id": "thr1", "turn_id": "turn1",
                      "room_id": room},
         "status": status, "execution_generation": gen}
    if payload:
        p = {}
        if member is not None:
            p["target_member_id"] = member
        if profile is not None:
            p["target_profile"] = profile
        d["payload"] = p
    return d


def callable_marker(marker):
    return {"__callable__": marker}


# ── 1. task_member_id ────────────────────────────────────────────────────────
add("task_member_id", {"task": task(member="mem1")})
add("task_member_id", {"task": task(member="", profile="prof")})
add("task_member_id", {"task": task(member=None, profile="prof2")})
add("task_member_id", {"task": task(member=0, profile="prof3")})
add("task_member_id", {"task": {"status": "running"}})                       # KeyError payload
add("task_member_id", {"task": {"payload": {}}})                              # KeyError target_profile
add("task_member_id", {"task": task(member=42)})
add("task_member_id", {"task": task(member=True)})

# ── 2. _approvable_attempts ──────────────────────────────────────────────────
add("approvable_attempts", {"tasks": [
    task(member="m1", status="running", tid="a"),
    task(member="m2", status="deferred", tid="b", gen=3),
    task(member="m3", status="settled", tid="c")]})
add("approvable_attempts", {"tasks": [
    task(member="m1", status="running", tid="a"),
    task(member="m1", status="running", tid="a")]})                           # dedupe
add("approvable_attempts", {"tasks": [task(member="m1", status="settled")]})  # empty
add("approvable_attempts", {"tasks": [{"identity": {}, "payload": {}}]})      # KeyError status
add("approvable_attempts", {"tasks": [{"status": "running", "payload": {}, "execution_generation": 1}]})  # KeyError identity
add("approvable_attempts", {"tasks": [
    task(member=None, profile="p1", status="indeterminate", tid="z", gen=9)]})
add("approvable_attempts", {"tasks": []})
add("approvable_attempts", {"tasks": [task(member="m", status="running")]})

# ── 3. attempt_action ────────────────────────────────────────────────────────
add("attempt_action", {"kind": "retry", "task": task(member="m1")})
add("attempt_action", {"kind": "discard", "task": task(member="m2", tid="t2", gen=4)})
add("attempt_action", {"kind": "retry", "task": {"payload": {}, "execution_generation": 1}})  # KeyError identity
add("attempt_action", {"kind": "retry", "task": {"payload": {}, "identity": {}, "execution_generation": 1}})  # AttributeError task_id
add("attempt_action", {"kind": "retry", "task": {"payload": {}, "identity": {"task_id": "t"}}})  # KeyError execution_generation
add("attempt_action", {"kind": "retry", "task": task(member=None, profile="p", tid="t3")})
add("attempt_action", {"kind": "retry", "task": {"status": "running", "execution_generation": 1}})  # KeyError payload

# ── 4. _member_of ────────────────────────────────────────────────────────────
add("member_of", {"task": task(member="m1")})
add("member_of", {"task": task(member="", profile="p1")})
add("member_of", {"task": {"status": "running"}})
add("member_of", {"task": {"payload": None, "status": "running"}})
add("member_of", {"task": {"payload": {}, "status": "running"}})
add("member_of", {"task": task(member=7)})
add("member_of", {"task": task(member=False, profile="p2")})

# ── 5. _authority ────────────────────────────────────────────────────────────
add("authority", {"room": {"authority_gateway_id": "gw1", "authority_epoch": 3}})
add("authority", {"room": {"authority_gateway_id": 5, "authority_epoch": 2}})
add("authority", {"room": {"authority_gateway_id": "gw1", "authority_epoch": "7"}})
add("authority", {"room": {"authority_epoch": 1}})                            # KeyError gid
add("authority", {"room": {"authority_gateway_id": "gw1"}})                    # KeyError epoch
add("authority", {"room": {"authority_gateway_id": "gw1", "authority_epoch": "abc"}})  # ValueError
add("authority", {"room": {"authority_gateway_id": "gw1", "authority_epoch": 2.9}})
add("authority", {"room": {"authority_gateway_id": "gw1", "authority_epoch": True}})

# ── 6. _grant_revoke_is_terminal ─────────────────────────────────────────────
add("grant_revoke_is_terminal", {"exc": {"status_code": 401, "error_code": "invalid_room_grant"}})
add("grant_revoke_is_terminal", {"exc": {"status_code": 403, "error_code": "room_reauthorization_required"}})
add("grant_revoke_is_terminal", {"exc": {"status_code": 500, "error_code": "invalid_room_grant"}})
add("grant_revoke_is_terminal", {"exc": {"status_code": 401, "error_code": "other"}})
add("grant_revoke_is_terminal", {"exc": {"status_code": "401", "error_code": "invalid_room_grant"}})
add("grant_revoke_is_terminal", {"exc": {"error_code": "invalid_room_grant"}})  # AttributeError status_code
add("grant_revoke_is_terminal", {"exc": {"status_code": 401}})                  # AttributeError error_code
add("grant_revoke_is_terminal", {"exc": {"status_code": 403.0, "error_code": "invalid_room_grant"}})

# ── 7. _hosted_room_turn_timeout_seconds ─────────────────────────────────────
add("hosted_room_turn_timeout_seconds", {}, {"os.getenv": "1800"})
add("hosted_room_turn_timeout_seconds", {}, {"os.getenv": "60"})
add("hosted_room_turn_timeout_seconds", {}, {"os.getenv": "0"})
add("hosted_room_turn_timeout_seconds", {}, {"os.getenv": "-5"})
add("hosted_room_turn_timeout_seconds", {}, {"os.getenv": "abc"})
add("hosted_room_turn_timeout_seconds", {}, {"os.getenv": None})
add("hosted_room_turn_timeout_seconds", {}, {"os.getenv": rs("ValueError", "bad env")})
add("hosted_room_turn_timeout_seconds", {}, {"os.getenv": rs("KeyboardInterrupt", "stop")})
add("hosted_room_turn_timeout_seconds", {}, {"os.getenv": rs("ImportError", "sibling")})

# ── 8. _hook ─────────────────────────────────────────────────────────────────
add("hook", {"obj": {"on_ready": callable_marker("ready")}, "name": "on_ready"})
add("hook", {"obj": {"on_ready": callable_marker("ready")}, "name": "missing"})
add("hook", {"obj": {"on_ready": "plain"}, "name": "on_ready"})
add("hook", {"obj": {}, "name": "x"})
add("hook", {"obj": {"x": callable_marker("c1"), "y": callable_marker("c2")}, "name": "y"})
add("hook", {"obj": None, "name": "x"})
add("hook", {"obj": {"x": callable_marker("c3")}, "name": "x"})

# ── 9. disband_fence ─────────────────────────────────────────────────────────
add("disband_fence", {"service": {"disbanding": callable_marker("disbanding")}, "room_id": "r1"},
    {"disbanding": "fence-ok"})
add("disband_fence", {"service": {}, "room_id": "r1"})
add("disband_fence", {"service": {"disbanding": None}, "room_id": "r1"})
add("disband_fence", {"service": {"disbanding": "plain"}, "room_id": "r1"})
add("disband_fence", {"service": {"disbanding": callable_marker("disbanding")}, "room_id": "r2"},
    {"disbanding": rs("RuntimeError", "boom")})
add("disband_fence", {"service": {"disbanding": callable_marker("disbanding")}, "room_id": "r3"},
    {"disbanding": rs("KeyboardInterrupt", "stop")})
add("disband_fence", {"service": {"disbanding": callable_marker("disbanding")}, "room_id": "r4"},
    {"disbanding": "ok4"})

# ── 10. route_publication_lock ───────────────────────────────────────────────
add("route_publication_lock", {"service": {"peer_route_lock": "L1"}})
add("route_publication_lock", {"service": {}})
add("route_publication_lock", {"service": {"peer_route_lock": None}})
add("route_publication_lock", {"service": {"peer_route_lock": {"id": 1}}})
add("route_publication_lock", {"service": {"peer_route_lock": False}})
add("route_publication_lock", {"service": {"peer_route_lock": 0}})
add("route_publication_lock", {"service": {"peer_route_lock": "L2"}})

# ── 11. _member_activity ─────────────────────────────────────────────────────
_t = task(member="m1", tid="t1", gen=2, started=123)
add("member_activity", {"task": _t},
    {"discussion.turn_round": 2, "task_progress": {"tool": "x"}})
add("member_activity", {"task": task(member="m2", tid="t2")},
    {"discussion.turn_round": None, "task_progress": {"tool": "y", "n": 3}})
_t3 = task(member="m3", tid="t3")
_t3["started_at"] = None
add("member_activity", {"task": _t3},
    {"discussion.turn_round": 1, "task_progress": {}})
add("member_activity", {"task": task(member="m4", tid="t4", started=99)},
    {"discussion.turn_round": 0, "task_progress": {"k": "v"}})
add("member_activity", {"task": task(member="m5", tid="t5")},
    {"discussion.turn_round": rs("RuntimeError", "turn"), "task_progress": {}})
add("member_activity", {"task": task(member="m6", tid="t6")},
    {"discussion.turn_round": 1, "task_progress": rs("ValueError", "progress")})
add("member_activity", {"task": {"status": "running", "execution_generation": 1, "payload": {}}})
add("member_activity", {"task": {"identity": {"task_id": "t"}, "execution_generation": 1, "payload": {}}})
_t9 = task(member="m9", tid="t9")
_t9["execution_generation"] = "3"
add("member_activity", {"task": _t9},
    {"discussion.turn_round": 2, "task_progress": {}})

# ── 12. _attempt_actions ─────────────────────────────────────────────────────
add("attempt_actions", {"tasks_shown": [
    task(member="m1", status="deferred", tid="a"),
    task(member="m2", status="settled", tid="b"),
    task(member="m3", status="indeterminate", tid="c", gen=4),
    task(member="m4", status="running", tid="d")]})
add("attempt_actions", {"tasks_shown": []})
add("attempt_actions", {"tasks_shown": [{"identity": {}, "payload": {}}]})
add("attempt_actions", {"tasks_shown": [task(member="m1", status="deferred")]})
add("attempt_actions", {"tasks_shown": [
    task(member=None, profile="p1", status="deferred", tid="z")]})
add("attempt_actions", {"tasks_shown": [task(member="m2", status="running")]})
add("attempt_actions", {"tasks_shown": [
    task(member="m1", status="indeterminate", tid="a"),
    task(member="m2", status="deferred", tid="b")]})

# ── 13. _stop_pending_refusal ────────────────────────────────────────────────
for i in range(6):
    add("stop_pending_refusal", {"room_id": "r%d" % i, "deferred": [{"status": "deferred"}]})

# ── 14. _policy_snapshot ─────────────────────────────────────────────────────
add("policy_snapshot", {"room": {"room_id": "r1", "latest_seq": 5}},
    {"policy_checkpoint.snapshot": {"a": 1}})
add("policy_snapshot", {"room": {"room_id": "r2", "latest_seq": "7"}},
    {"policy_checkpoint.snapshot": {"b": 2}})
add("policy_snapshot", {"room": {"latest_seq": 5}})                          # KeyError room_id
add("policy_snapshot", {"room": {"room_id": "r1"}})                           # KeyError latest_seq
add("policy_snapshot", {"room": {"room_id": "r1", "latest_seq": "x"}})        # ValueError
add("policy_snapshot", {"room": {"room_id": "r1", "latest_seq": 5}},
    {"policy_checkpoint.snapshot": rs("RuntimeError", "snap")})
add("policy_snapshot", {"room": {"room_id": 9, "latest_seq": 1}},
    {"policy_checkpoint.snapshot": None})

# ── 15. profile_homes ────────────────────────────────────────────────────────
add("profile_homes", {"self": {"root": "/home/u"}}, {"self.local_profiles": [["default", "alpha"]]})
add("profile_homes", {"self": {"root": ""}}, {"self.local_profiles": [["default", "alpha"]]})
add("profile_homes", {"self": {"root": "/home/u"}}, {"self.local_profiles": [[]]})
add("profile_homes", {"self": {"root": "/home/u"}}, {"self.local_profiles": [["default"]]})
add("profile_homes", {"self": {"root": "/home/u"}},
    {"self.local_profiles": rs("RuntimeError", "profiles")})
add("profile_homes", {"self": {"root": "/home/u"}}, {"self.local_profiles": [["beta", "gamma"]]})
add("profile_homes", {"self": {"root": "/home/u/"}}, {"self.local_profiles": [["default", "alpha"]]})

# ── 16. _route_statuses ──────────────────────────────────────────────────────
_STATES = [
    {"room_id": "r2", "member_id": "b", "status": "ready"},
    {"room_id": "r1", "member_id": "z", "status": "unavailable"},
    {"room_id": "r1", "member_id": "a", "status": "ready"},
]
add("route_statuses", {"self": {"_peer_route_status": _STATES}})
add("route_statuses", {"self": {"_peer_route_status": _STATES}, "room_id": "r1"})
add("route_statuses", {"self": {"_peer_route_status": _STATES}, "room_id": None})
add("route_statuses", {"self": {"_peer_route_status": []}})
add("route_statuses", {"self": {"_peer_route_status": _STATES}, "room_id": "r9"})
add("route_statuses", {"self": {}})
add("route_statuses", {"self": {"_peer_route_status": _STATES}, "room_id": "r2"})

# ── 17. _events ──────────────────────────────────────────────────────────────
add("events", {"room_id": "r1"},
    {"hosted_rooms.read_events": {"events": [{"seq": 1}], "cursor": 1, "has_more": False}})
add("events", {"room_id": "r2"}, {"hosted_rooms.read_events": [
    {"events": [{"seq": 1}], "cursor": 2, "has_more": True},
    {"events": [{"seq": 2}], "cursor": 2, "has_more": False}]})
add("events", {"room_id": "r3"},
    {"hosted_rooms.read_events": {"events": "bad", "cursor": 1, "has_more": False}})
add("events", {"room_id": "r4"},
    {"hosted_rooms.read_events": {"events": [{"seq": 1}, 5], "cursor": 1, "has_more": False}})
add("events", {"room_id": "r5"},
    {"hosted_rooms.read_events": {"events": [], "cursor": 0, "has_more": True}})
add("events", {"room_id": "r6"}, {"hosted_rooms.read_events": rs("OSError", "io")})
add("events", {"room_id": "r7"},
    {"hosted_rooms.read_events": {"events": [{"seq": 1}], "cursor": "2", "has_more": False}})
add("events", {"room_id": "r8"},
    {"hosted_rooms.read_events": {"events": [{"seq": 1}], "cursor": None, "has_more": False}})

# ── 18. _append_room_status ──────────────────────────────────────────────────
_ROOM = {"room_id": "r1", "authority_gateway_id": "gw1", "authority_epoch": 3}
add("append_room_status", {"room": _ROOM,
    "decision": {"discussion_event_id": "d1", "continues": 0, "reason": "settled",
                 "status": "settled", "thread_id": "thr1"}},
    {"hosted_rooms.append_event": None})
add("append_room_status", {"room": _ROOM,
    "decision": {"discussion_event_id": "d2", "continues": 2, "reason": "bounded",
                 "status": "bounded", "thread_id": "thr2"}},
    {"hosted_rooms.append_event": None})
add("append_room_status", {"room": _ROOM,
    "decision": {"discussion_event_id": None, "continues": 0, "reason": "x",
                 "status": "settled", "thread_id": "thr1"}})
add("append_room_status", {"room": {"room_id": "r1"},
    "decision": {"discussion_event_id": "d1", "continues": 0, "reason": "x",
                 "status": "settled", "thread_id": "thr1"}})
add("append_room_status", {"room": {"room_id": "r1", "authority_gateway_id": "gw1", "authority_epoch": "bad"},
    "decision": {"discussion_event_id": "d1", "continues": 0, "reason": "x",
                 "status": "settled", "thread_id": "thr1"}})
add("append_room_status", {"room": _ROOM,
    "decision": {"discussion_event_id": "d3", "continues": 0, "reason": "x",
                 "status": "settled", "thread_id": "thr1"}},
    {"hosted_rooms.append_event": rs("RuntimeError", "append")})
add("append_room_status", {"room": _ROOM,
    "decision": {"discussion_event_id": "d4", "continues": True, "reason": "x",
                 "status": "settled", "thread_id": "thr1"}},
    {"hosted_rooms.append_event": None})

# ── 19. publish_terminal ─────────────────────────────────────────────────────
_BINDING = {"room_id": "r1"}
add("publish_terminal", {"binding": _BINDING, "task": task(member="m1", profile="p1", status="deferred", tid="t1")},
    {"self._member_is_peer": True, "forget_task_progress": None,
     "self._prepare_tracked": None, "self.runtime.wakeup": None})
add("publish_terminal", {"binding": _BINDING, "task": task(member="m1", profile="p1", status="deferred", tid="t1")},
    {"self._member_is_peer": False, "forget_task_progress": None,
     "self._prepare_tracked": None, "self.runtime.wakeup": None})
add("publish_terminal", {"binding": _BINDING, "task": task(member="m1", status="running", tid="t1")},
    {"forget_task_progress": None, "self._prepare_tracked": None, "self.runtime.wakeup": None})
add("publish_terminal", {"binding": _BINDING, "task": task(member="m1", status="settled", tid="t1")},
    {"forget_task_progress": None, "self._prepare_tracked": None, "self.runtime.wakeup": None})
add("publish_terminal", {"binding": _BINDING, "task": task(member="m1", profile="p1", status="deferred", tid="t1")},
    {"self._member_is_peer": rs("RuntimeError", "peer")})
add("publish_terminal", {"binding": _BINDING, "task": task(member="m1", profile="p1", status="deferred", tid="t1")},
    {"self._member_is_peer": False, "forget_task_progress": rs("ValueError", "forget")})
add("publish_terminal", {"binding": _BINDING, "task": task(member="m1", status="running", tid="t1")},
    {"forget_task_progress": None, "self._prepare_tracked": rs("RuntimeError", "prep")})
add("publish_terminal", {"binding": _BINDING, "task": task(member="m1", status="running", tid="t1")},
    {"forget_task_progress": None, "self._prepare_tracked": None,
     "self.runtime.wakeup": rs("RuntimeError", "wake")})
add("publish_terminal", {"binding": _BINDING, "task": {"identity": {"task_id": "t"}, "execution_generation": 1, "payload": {}}})

# ── 20. peer_admission_receipt ───────────────────────────────────────────────
_ROUTE = {"room_id": "r1", "member_id": "m1", "home_install_id": "home1",
          "target_install_id": "tgt1", "target_profile": "p1"}
_ROOM = {"authority_gateway_id": "gw1", "authority_epoch": 3}
add("peer_admission_receipt", {"self": {"peer_routes": [_ROUTE]}, "room_id": "r1",
    "task": task(member="m1", tid="t1", gen=2)},
    {"self._room": _ROOM, "hosted_rooms.remote_run_receipt": {"run_id": "run1"}})
add("peer_admission_receipt", {"self": {"peer_routes": []}, "room_id": "r1",
    "task": task(member="m1")})
add("peer_admission_receipt", {"self": {"peer_routes": [_ROUTE]}, "room_id": "r1",
    "task": task(member="m1")},
    {"self._room": rs("RuntimeError", "room")})
add("peer_admission_receipt", {"self": {"peer_routes": [_ROUTE]}, "room_id": "r1",
    "task": task(member="m1")},
    {"self._room": _ROOM, "hosted_rooms.remote_run_receipt": rs("ValueError", "receipt")})
add("peer_admission_receipt", {"self": {"peer_routes": [_ROUTE]}, "room_id": "r1",
    "task": task(member=None, profile="p1")},
    {"self._room": _ROOM, "hosted_rooms.remote_run_receipt": None})
add("peer_admission_receipt", {"self": {"peer_routes": [_ROUTE]}, "room_id": "r1",
    "task": {"identity": {}, "execution_generation": 1, "payload": {"target_member_id": "m1"}}})
add("peer_admission_receipt", {"self": {"peer_routes": [_ROUTE]}, "room_id": "r1",
    "task": task(member="m1")},
    {"self._room": {"authority_gateway_id": "gw1", "authority_epoch": "bad"},
     "hosted_rooms.remote_run_receipt": None})
_t8 = task(member="m1", tid="t1")
_t8["execution_generation"] = "2"
add("peer_admission_receipt", {"self": {"peer_routes": [_ROUTE]}, "room_id": "r1", "task": _t8},
    {"self._room": _ROOM, "hosted_rooms.remote_run_receipt": None})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for k in sorted(counts):
        print("  %-34s %d" % (k, counts[k]))
    thin = {k: v for k, v in counts.items() if v < 6}
    if thin:
        print("WARNING <6 cases: %s" % thin)


if __name__ == "__main__":
    main()
