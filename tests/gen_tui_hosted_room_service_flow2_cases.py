#!/usr/bin/env python3
"""Generate tests/fixtures/tui_hosted_room_service_flow2.jsonl — scripted-seam
FLOW2 cases for the 15-def tui_hosted_room_service_flow2 port.

Every case: {"func": <harness name>, "in": {... inputs ..., "seams": {name: script}}}
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). {"__raise__": cls, "msg": m} raises that exception class in the live
drive and is delivered as a raise descriptor to the C universal seam. A MISSING
seam key fail-closes (RuntimeError) on BOTH sides.

The seam-name set is EXACTLY (C calls == oracle stubs == fixture keys):
  live_services.snapshot, self._room, local_authority_gateway_id, list_rooms,
  driver.list_tasks, acquire_turn_lock, self.runtime.start, self.runtime.stop,
  self.runtime.wakeup, hosted_room_links.make_stored_link,
  hosted_room_links.save_room_link, approve_receipt, rpc.approve,
  self.local_profiles, HostedRoomServerRPC, mark_room_link_status

Domains: ASCII strings; Path roots are non-empty normalized POSIX paths (so
str(PurePosixPath(root)) == input root); attribute doubles for self/client/
route; str()/int() only on scalars. Run: python tests/gen_tui_hosted_room_service_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "tui_hosted_room_service_flow2.jsonl")
CASES = []


def add(func, inp, seams=None):
    inp = dict(inp)
    inp["seams"] = seams or {}
    CASES.append({"func": func, "in": inp})


def rs(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def callable_marker(marker):
    return {"__callable__": marker}


def room(members=None, gid="gw1", epoch=3):
    return {"authority_gateway_id": gid, "authority_epoch": epoch,
            "members": members if members is not None else []}


def member(mid=None, profile=None, target_kind=None):
    d = {}
    if mid is not None:
        d["member_id"] = mid
    if profile is not None:
        d["profile"] = profile
    if target_kind is not None:
        d["target"] = {"kind": target_kind}
    return d


# ── 1. live_room_services ─────────────────────────────────────────────────────
add("live_room_services", {}, {"live_services.snapshot": ["svc1", "svc2"]})
add("live_room_services", {}, {"live_services.snapshot": []})
add("live_room_services", {}, {"live_services.snapshot": ["only"]})
add("live_room_services", {}, {"live_services.snapshot": rs("RuntimeError", "weakset")})
add("live_room_services", {}, {"live_services.snapshot": [1, 2, 3]})
add("live_room_services", {}, {"live_services.snapshot": ["a", "b", "a"]})
add("live_room_services", {}, {"live_services.snapshot": ["s"]})

# ── 2. member_is_peer ─────────────────────────────────────────────────────────
add("member_is_peer", {"room_id": "r1", "member_id": "m1"},
    {"self._room": room([member("m1", target_kind="peer")])})
add("member_is_peer", {"room_id": "r1", "member_id": "m1"},
    {"self._room": room([member("m1", target_kind="local")])})
add("member_is_peer", {"room_id": "r1", "member_id": "m1"},
    {"self._room": room([member(profile="m1", target_kind="peer")])})
add("member_is_peer", {"room_id": "r1", "member_id": "nope"},
    {"self._room": room([member("m1", target_kind="peer")])})
add("member_is_peer", {"room_id": "r1", "member_id": "m1"},
    {"self._room": room([member("m1")])})
add("member_is_peer", {"room_id": "r1", "member_id": "m1"},
    {"self._room": room(["not-a-mapping", member("m1", target_kind="peer")])})
add("member_is_peer", {"room_id": "r1", "member_id": "m1"}, {"self._room": room([])})
add("member_is_peer", {"room_id": "r1", "member_id": "m1"},
    {"self._room": rs("OSError", "io")})
add("member_is_peer", {"room_id": "r1", "member_id": "7"},
    {"self._room": room([member(7, target_kind="peer")])})

# ── 3. owned_authority ────────────────────────────────────────────────────────
add("owned_authority", {"room_id": "r1"},
    {"self._room": {"authority_gateway_id": "gw1", "authority_epoch": 3},
     "local_authority_gateway_id": "gw1"})
add("owned_authority", {"room_id": "r1"},
    {"self._room": {"authority_gateway_id": "gw2", "authority_epoch": 3},
     "local_authority_gateway_id": "gw1"})
add("owned_authority", {"room_id": "r1"},
    {"self._room": {"authority_epoch": 3}, "local_authority_gateway_id": "gw1"})
add("owned_authority", {"room_id": "r1"},
    {"self._room": {"authority_gateway_id": "gw1"}, "local_authority_gateway_id": "gw1"})
add("owned_authority", {"room_id": "r1"},
    {"self._room": {"authority_gateway_id": "gw1", "authority_epoch": "bad"},
     "local_authority_gateway_id": "gw1"})
add("owned_authority", {"room_id": "r1"},
    {"self._room": {"authority_gateway_id": 5, "authority_epoch": 2},
     "local_authority_gateway_id": "5"})
add("owned_authority", {"room_id": "r1"},
    {"self._room": rs("RuntimeError", "down")})

# ── 4. route_authority ────────────────────────────────────────────────────────
add("route_authority", {"self": {"_disbanding_rooms": ["r1"]}, "room_id": "r1"})
add("route_authority", {"self": {"_disbanding_rooms": ["r2"]}, "room_id": "r1"},
    {"self._room": {"authority_gateway_id": "gw1", "authority_epoch": 3},
     "local_authority_gateway_id": "gw1"})
add("route_authority", {"self": {"_disbanding_rooms": []}, "room_id": "r1"},
    {"self._room": {"authority_gateway_id": "gw1", "authority_epoch": 3},
     "local_authority_gateway_id": "gw1"})
add("route_authority", {"self": {}, "room_id": "r1"},
    {"self._room": {"authority_gateway_id": "gw2", "authority_epoch": 3},
     "local_authority_gateway_id": "gw1"})
add("route_authority", {"self": {}, "room_id": "r1"},
    {"self._room": {"authority_gateway_id": "gw1", "authority_epoch": "x"},
     "local_authority_gateway_id": "gw1"})
add("route_authority", {"self": {"_disbanding_rooms": ["r2", "r3"]}, "room_id": "r1"},
    {"self._room": {"authority_gateway_id": "gw1", "authority_epoch": 1},
     "local_authority_gateway_id": "gw1"})
add("route_authority", {"self": {"_disbanding_rooms": ["r1"]}, "room_id": "r1"},
    {"self._room": rs("RuntimeError", "should-not-fire")})

# ── 5. bindings ───────────────────────────────────────────────────────────────
add("bindings", {}, {"local_authority_gateway_id": "gw1", "list_rooms": [
    {"room_id": "r1", "authority_gateway_id": "gw1", "authority_epoch": 3},
    {"room_id": "r2", "authority_gateway_id": "gw2", "authority_epoch": 1}]})
add("bindings", {}, {"local_authority_gateway_id": "gw1", "list_rooms": [
    {"room_id": "r1", "authority_gateway_id": "gw1", "authority_epoch": 3}]})
add("bindings", {}, {"local_authority_gateway_id": "gw1", "list_rooms": [
    {"room_id": "r2", "authority_gateway_id": "gw2", "authority_epoch": 1}]})
add("bindings", {}, {"local_authority_gateway_id": "gw1", "list_rooms": []})
add("bindings", {}, {"local_authority_gateway_id": "gw1", "list_rooms": [
    {"room_id": "r1", "authority_epoch": 3}]})
add("bindings", {}, {"local_authority_gateway_id": "gw1", "list_rooms": [
    {"room_id": "r1", "authority_gateway_id": "gw1"}]})
add("bindings", {}, {"local_authority_gateway_id": "gw1", "list_rooms": [
    {"room_id": "r1", "authority_gateway_id": "gw1", "authority_epoch": "bad"}]})
add("bindings", {}, {"local_authority_gateway_id": "gw1", "list_rooms": rs("OSError", "list")})

# ── 6. list_tasks ─────────────────────────────────────────────────────────────
add("list_tasks", {"room_id": "r1", "statuses": ["running", "deferred"]},
    {"driver.list_tasks": [[{"task_id": "a"}], [{"task_id": "b"}, {"task_id": "c"}]]})
add("list_tasks", {"room_id": "r1", "statuses": []}, {"driver.list_tasks": [[{"task_id": "a"}]]})
add("list_tasks", {"room_id": "r1", "statuses": ["running"]},
    {"driver.list_tasks": [[{"task_id": "a"}]]})
add("list_tasks", {"room_id": "r1", "statuses": ["running", "deferred"]},
    {"driver.list_tasks": [rs("RuntimeError", "driver")]})
add("list_tasks", {"room_id": "r1", "statuses": ["a", "b", "c"]},
    {"driver.list_tasks": [[], [{"task_id": "x"}], []]})
add("list_tasks", {"room_id": "r2", "statuses": ["queued"]},
    {"driver.list_tasks": [[{"task_id": "q", "status": "queued"}]]})
add("list_tasks", {"room_id": "r1", "statuses": ["running", "deferred"]},
    {"driver.list_tasks": [[{"task_id": "a"}], rs("ValueError", "second")]})

# ── 7. turn_lock ──────────────────────────────────────────────────────────────
add("turn_lock", {"self": {"root": "/home/u"}, "profile": "default"},
    {"acquire_turn_lock": "LOCK1"})
add("turn_lock", {"self": {"root": "/home/u"}, "profile": "alpha"},
    {"acquire_turn_lock": None})
add("turn_lock", {"self": {"root": "/home/u"}, "profile": "default"},
    {"acquire_turn_lock": rs("RuntimeError", "busy")})
add("turn_lock", {"self": {"root": "/srv/root"}, "profile": "beta"},
    {"acquire_turn_lock": {"id": 1}})
add("turn_lock", {"self": {"root": "/home/u"}, "profile": ""},
    {"acquire_turn_lock": "LOCK2"})
add("turn_lock", {"self": {"root": "/home/u"}, "profile": "gamma"},
    {"acquire_turn_lock": rs("KeyboardInterrupt", "stop")})

# ── 8. start ──────────────────────────────────────────────────────────────────
add("start", {}, {"self.runtime.start": None})
add("start", {}, {"self.runtime.start": rs("RuntimeError", "start")})
add("start", {}, {"self.runtime.start": "ignored"})
add("start", {}, {"self.runtime.start": rs("KeyboardInterrupt", "stop")})
add("start", {}, {"self.runtime.start": True})
# SystemExit("bye") carries .code == "bye" in live; the seed descriptor must
# carry it too so both sides serialize the raised exception identically
# (C harness thrs2_norm_raise copies "code" when present).
add("start", {}, {"self.runtime.start": {"__raise__": "SystemExit", "msg": "bye", "code": "bye"}})

# ── 9. stop ───────────────────────────────────────────────────────────────────
add("stop", {}, {"self.runtime.stop": True})
add("stop", {}, {"self.runtime.stop": False})
add("stop", {"timeout": 0.5}, {"self.runtime.stop": True})
add("stop", {"timeout": 9}, {"self.runtime.stop": False})
add("stop", {}, {"self.runtime.stop": rs("RuntimeError", "stop")})
add("stop", {}, {"self.runtime.stop": None})
add("stop", {"timeout": 2.5}, {"self.runtime.stop": rs("KeyboardInterrupt", "stop")})

# ── 10. wakeup ────────────────────────────────────────────────────────────────
add("wakeup", {}, {"self.runtime.wakeup": None})
add("wakeup", {}, {"self.runtime.wakeup": rs("RuntimeError", "wake")})
add("wakeup", {}, {"self.runtime.wakeup": "ignored"})
add("wakeup", {}, {"self.runtime.wakeup": rs("KeyboardInterrupt", "stop")})
add("wakeup", {}, {"self.runtime.wakeup": 1})
add("wakeup", {}, {"self.runtime.wakeup": rs("GeneratorExit", "gen")})

# ── 11. save_link ─────────────────────────────────────────────────────────────
add("save_link", {"link": {"room_id": "r1", "grant": "g1"}},
    {"hosted_room_links.make_stored_link": "S1",
     "hosted_room_links.save_room_link": None})
add("save_link", {"link": {}},
    {"hosted_room_links.make_stored_link": "S2",
     "hosted_room_links.save_room_link": None})
add("save_link", {"link": {"room_id": "r2"}},
    {"hosted_room_links.make_stored_link": rs("ValueError", "make"),
     "hosted_room_links.save_room_link": None})
add("save_link", {"link": {"room_id": "r3"}},
    {"hosted_room_links.make_stored_link": "S3",
     "hosted_room_links.save_room_link": rs("OSError", "save")})
add("save_link", {"link": {"room_id": "r4", "grant": "g4"}},
    {"hosted_room_links.make_stored_link": {"id": 7},
     "hosted_room_links.save_room_link": {"ok": True}})
add("save_link", {"link": {"room_id": "r5"}},
    {"hosted_room_links.make_stored_link": rs("KeyboardInterrupt", "stop")})

# ── 12. dispatch_approval ─────────────────────────────────────────────────────
_AP = {"task_id": "t1", "execution_generation": 2, "request_id": "req1", "choice": "once"}
add("dispatch_approval", {**_AP, "route": {"grant": "g1"},
    "client": {"approve_receipt": callable_marker("approve_receipt")},
    "action": {"session_id": "s1"}}, {"approve_receipt": {"ok": True}})
add("dispatch_approval", {**_AP, "route": {"grant": "g1"},
    "client": {"approve_receipt": callable_marker("approve_receipt")},
    "action": {"session_id": "s1"}}, {"approve_receipt": rs("RuntimeError", "approve")})
add("dispatch_approval", {**_AP, "route": None, "client": None,
    "action": {"session_id": "s1"}}, {"rpc.approve": {"ok": 1}})
add("dispatch_approval", {**_AP, "route": None,
    "client": {"approve_receipt": "plain"}, "action": {"session_id": "s2"}},
    {"rpc.approve": rs("ValueError", "rpc")})
add("dispatch_approval", {**_AP, "route": None, "client": None, "action": {}},
    {"rpc.approve": None})
add("dispatch_approval", {**_AP, "route": None, "client": None,
    "action": {"session_id": ""}}, {"rpc.approve": None})
add("dispatch_approval", {**_AP, "route": {"grant": "g2"},
    "client": {"approve_receipt": callable_marker("approve_receipt")},
    "action": {"session_id": "s1"}}, {"approve_receipt": None})

# ── 13. room_profiles ─────────────────────────────────────────────────────────
add("room_profiles", {}, {"self.local_profiles": ["default", "alpha"]})
add("room_profiles", {}, {"self.local_profiles": []})
add("room_profiles", {}, {"self.local_profiles": rs("RuntimeError", "profiles")})
add("room_profiles", {}, {"self.local_profiles": ["default"]})
add("room_profiles", {}, {"self.local_profiles": "not-a-list"})
add("room_profiles", {}, {"self.local_profiles": ["a", "b", "default"]})

# ── 14. make_rpc ──────────────────────────────────────────────────────────────
add("make_rpc", {"self": {"db_path": "/db", "root": "/home/u", "placement": "shared"},
    "server": "srv1"}, {"HostedRoomServerRPC": "RPC1"})
add("make_rpc", {"self": {"db_path": "/db", "root": "/home/u", "placement": "local"},
    "server": "srv2"}, {"HostedRoomServerRPC": None})
add("make_rpc", {"self": {"db_path": "/db", "root": "/home/u", "placement": "shared"},
    "server": "srv1"}, {"HostedRoomServerRPC": rs("RuntimeError", "rpc")})
add("make_rpc", {"self": {"db_path": "/db2", "root": "/srv/r", "placement": "shared"},
    "server": {"host": "h"}}, {"HostedRoomServerRPC": {"id": 1}})
add("make_rpc", {"self": {"db_path": "", "root": "/home/u", "placement": "shared"},
    "server": "srv3"}, {"HostedRoomServerRPC": "RPC3"})
add("make_rpc", {"self": {"db_path": "/db", "root": "/home/u", "placement": "shared"},
    "server": "srv4"}, {"HostedRoomServerRPC": rs("KeyboardInterrupt", "stop")})

# ── 15. set_route_status ──────────────────────────────────────────────────────
_ST = [{"room_id": "r1", "member_id": "m1", "status": "ready"}]
add("set_route_status", {"self": {"_peer_route_status": _ST},
    "room_id": "r1", "member_id": "m1", "status": "ready"},
    {"mark_room_link_status": None})
add("set_route_status", {"self": {"_peer_route_status": _ST},
    "room_id": "r1", "member_id": "m1", "status": "unavailable"},
    {"mark_room_link_status": None})
add("set_route_status", {"self": {"_peer_route_status": _ST},
    "room_id": "r1", "member_id": "m2", "status": "ready"},
    {"mark_room_link_status": None})
add("set_route_status", {"self": {}, "room_id": "r1", "member_id": "m1", "status": "ready"},
    {"mark_room_link_status": None})
add("set_route_status", {"self": {"_peer_route_status": _ST},
    "room_id": "r1", "member_id": "m1", "status": "ready"},
    {"mark_room_link_status": rs("OSError", "should-not-fire")})
add("set_route_status", {"self": {"_peer_route_status": _ST},
    "room_id": "r2", "member_id": "m1", "status": "ready"},
    {"mark_room_link_status": rs("RuntimeError", "mark")})


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
        print("  %-24s %d" % (k, counts[k]))
    thin = {k: v for k, v in counts.items() if v < 6}
    if thin:
        print("WARNING <6 cases: %s" % thin)


if __name__ == "__main__":
    main()
