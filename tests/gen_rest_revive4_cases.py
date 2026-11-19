#!/usr/bin/env python3
"""gen_rest_revive4_cases.py — regenerate tests/fixtures/rest_revive4.jsonl.

Models are normalized to the FULL dataclass field sets (members carry
display_name/target, events carry raw/actor/payload) because the C seam args
pass the scripted models through unnormalized while the oracle projects
dataclasses with asdict. Reference task plans are minted with the LIVE
_make_task_plan so reconstruction/publication round-trips are exact.
"""
import copy
import json
import sys
from dataclasses import asdict
from pathlib import Path

TESTS = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS))
import oracle_rest_revive4 as o  # noqa: E402

M1 = {"member_id": "m1", "profile": "default", "handle": "alice",
      "display_name": "Alice", "target": {"kind": "local", "profile": "default"}}
M2 = {"member_id": "m2", "profile": "default", "handle": "bob",
      "display_name": "", "target": None}
ROOM = {"room_id": "room1", "name": "Demo Room", "members": [M1, M2],
        "gateway_id": "gw1", "authority_epoch": 7}


def ev(seq, event_id, kind, payload, actor=None, raw=None):
    return {"raw": raw if raw is not None else {}, "seq": seq, "event_id": event_id,
            "kind": kind, "actor": actor if actor is not None else {},
            "payload": payload}


USER1 = ev(1, "u1", "message.user", {"thread_id": "th1", "text": "hello @alice"})
USER2 = ev(2, "u2", "message.user", {"thread_id": "th1", "text": "newer"})
MM1 = ev(3, "mm1", "message.member",
         {"thread_id": "th1", "text": "answer", "member_id": "m1",
          "discussion_event_id": "u1", "round_index": 0, "task_id": "tX",
          "member_index": 0, "turn_id": "x"})
SETTLED1 = ev(4, "ts1", "turn.settled",
              {"thread_id": "th1", "member_id": "m1", "task_id": "tX",
               "discussion_event_id": "u1", "round_index": 0, "member_index": 0,
               "turn_id": "x", "seen_through_seq": 3,
               "message_event_id": "mm1", "passed": False})


def mint(member, prompt, seen, input_context=None, lead_routing=False, round_index=0,
         member_index=0, attachments=()):
    plan = o.NS_HRD["_make_task_plan"](
        room=o.room_double(ROOM), discussion_event=o.event_double(USER1),
        member=o.member_double(member), member_index=member_index,
        round_index=round_index, seen_through_seq=seen, prompt=prompt,
        input_context=input_context, attachments=attachments, lead_routing=lead_routing)
    return asdict(plan)


TASK1 = mint(M1, "hello", 1)
TASK_LEAD = mint(M1, "route please", 0, lead_routing=True)
TASK_IC = mint(M1, "hello", 1, input_context={"watermark": 0, "event_seqs": [1]})

CLAIMS = {"room_id": "r1", "home_install_id": "h1", "authority_gateway_id": "g1",
          "authority_epoch": 3, "member_id": "m1",
          "target_profile": "default", "target_install_id": "inst1"}
CLAIMS2 = dict(CLAIMS, expires_at=1000, status_expires_at=2000)
CLAIMS3 = dict(CLAIMS2, execution_policy_digest="pd1", permissions=["dispatch"])
BODY_INV = {"room_id": "r1", "home_install_id": "h1", "authority_gateway_id": "g1",
            "authority_epoch": 3, "member_id": "m1", "ttl_seconds": 3600}

CASES = []


def add(func, inp):
    CASES.append({"func": func, "in": inp})


def script(ret=None, raise_=None):
    d = {"__script__": True}
    if raise_ is not None:
        d["raise"] = raise_
    else:
        d["ret"] = ret
    return d


def rais(ty, msg=""):
    return {"type": ty, "message": msg}


# ── room_grant_token (pure) ──
for auth, want in [
    ("HermesRoom tok123", "tok123"), ("hermesroom  tok  ", "tok"),
    ("HERMESROOM x", "x"), ("Bearer x", ""), ("HermesRoom", ""),
    ("", ""), (None, ""), (42, ""), ("HermesRoom \u00a0tok\u00a0", "tok"),
]:
    add("room_grant_token", {"request": {"headers": {"Authorization": auth}}})

# ── room_grant_secret ──
add("room_grant_secret", {"self": {}, "io": {
    "import_gateway_room_grant_secret": None, "gateway_room_grant_secret": "s3cret"}})
add("room_grant_secret", {"self": {}, "io": {
    "import_gateway_room_grant_secret": script(raise_=rais("ImportError", "no module"))}})
add("room_grant_secret", {"self": {}, "io": {
    "import_gateway_room_grant_secret": None,
    "gateway_room_grant_secret": script(raise_=rais("ValueError", "bad secret"))}})
add("room_grant_secret", {"self": {}, "io": {
    "import_gateway_room_grant_secret": None,
    "gateway_room_grant_secret": script(raise_=rais("SystemExit", "bye"))}})
add("room_grant_secret", {"self": {}, "io": {
    "import_gateway_room_grant_secret": None, "gateway_room_grant_secret": ""}})

# ── run_idempotency_scope / durable_run_status / normalize_room_dispatch ──
add("run_idempotency_scope", {"self": {"id": "s"}, "request": {"path": "/p"},
                              "io": {"api_runs.run_idempotency_scope": "scope-abc"}})
add("run_idempotency_scope", {"self": None, "request": None,
                              "io": {"api_runs.run_idempotency_scope": script(raise_=rais("ValueError", "v"))}})
add("run_idempotency_scope", {"self": {}, "request": {},
                              "io": {"api_runs.run_idempotency_scope": script(raise_=rais("SystemExit", "x"))}})
add("run_idempotency_scope", {"self": {}, "request": {},
                              "io": {"api_runs.run_idempotency_scope": ""}})
add("run_idempotency_scope", {"self": {}, "request": {},
                              "io": {"api_runs.run_idempotency_scope": "a\0b"}})
for ret in [{"status": "running"}, None, script(raise_=rais("RuntimeError", "r")),
            script(raise_=rais("KeyboardInterrupt", "k")), {"nested": {"a": [1, 2]}}]:
    add("durable_run_status", {"self": {}, "request": {}, "run_id": "run1",
                               "io": {"api_runs.durable_run_status": ret}})
for ret in [script(ret=[{"input": "x"}, None]), script(raise_=rais("ValueError", "bad")),
            script(raise_=rais("SystemExit", "s")), script(ret=[{"a": 1}, {"error": True}]), script(ret=[None, None])]:
    add("normalize_room_dispatch", {"self": {}, "request": {}, "body": {"input": "x"},
                                    "io": {"room_dispatch.normalize_room_dispatch": ret}})

# ── declared_conversation_session ──
add("declared_conversation_session", {"gateway_session_key": "", "io": {}})
add("declared_conversation_session", {"gateway_session_key": "   ", "io": {}})
add("declared_conversation_session", {"gateway_session_key": "k1", "session_source": "api",
                                      "io": {"ensure_store": None}})
add("declared_conversation_session", {
    "gateway_session_key": " k1 ", "session_source": "api", "session_authority_present": True,
    "io": {"ensure_store": {"db": 1}, "import_declared_api_session": None,
           "store_beside": {"store": 1}, "declared_api_session": "sess-9"}})
add("declared_conversation_session", {
    "gateway_session_key": "k1", "session_source": "api",
    "io": {"ensure_store": {"db": 1}, "store_beside": None}})
add("declared_conversation_session", {
    "gateway_session_key": "k1", "session_source": "api",
    "io": {"ensure_store": {"db": 1}, "store_beside": {"store": 1},
           "latest_session_for_peer": {"session": {"session_id": "s1"}}}})
add("declared_conversation_session", {
    "gateway_session_key": "k1", "session_source": "api",
    "io": {"ensure_store": {"db": 1}, "store_beside": {"store": 1},
           "latest_session_for_peer": None}})
add("declared_conversation_session", {
    "gateway_session_key": "k1", "session_source": "api",
    "io": {"ensure_store": {"db": 1}, "store_beside": {"store": 1},
           "latest_session_for_peer": {"session": {"session_id": ""}}}})
add("declared_conversation_session", {
    "gateway_session_key": "k1", "session_source": "api",
    "io": {"ensure_store": {"db": 1}, "store_beside": {"store": 1},
           "latest_session_for_peer": {"session": {"session_id": 0}}}})
add("declared_conversation_session", {
    "gateway_session_key": "k1", "session_source": "api",
    "io": {"ensure_store": {"db": 1}, "store_beside": {"store": 1},
           "latest_session_for_peer": {"session": {"session_id": 123}}}})
add("declared_conversation_session", {
    "gateway_session_key": "k1", "session_source": "api",
    "io": {"ensure_store": {"db": 1}, "store_beside": {"store": 1},
           "latest_session_for_peer": script(raise_=rais("RuntimeError", "db down"))}})
add("declared_conversation_session", {
    "gateway_session_key": "k1", "session_source": "api",
    "io": {"ensure_store": {"db": 1}, "store_beside": {"store": 1},
           "latest_session_for_peer": script(raise_=rais("SystemExit", "halt"))}})
add("declared_conversation_session", {
    "gateway_session_key": "k1", "session_source": "api",
    "io": {"ensure_store": script(raise_=rais("OSError", "disk"))}})

# ── room_grant_claims ──
RG_BASE = {"room_grant_token": "tok", "import_decode_room_grant": None,
           "room_grant_secret": "sec", "decode_room_grant": CLAIMS,
           "import_hosted_rooms": None, "import_persistence_assembly": None,
           "profile_scope": "default", "open_persistence": {"store": 1},
           "room_grant_is_revoked": False, "peer_room_grant_is_current": True}


def rg_io(**over):
    io = copy.deepcopy(RG_BASE)
    io.update(over)
    return io


add("room_grant_claims", {"request": {}, "permission": "status", "io": rg_io()})
add("room_grant_claims", {"request": {}, "permission": "dispatch",
                          "io": rg_io(room_grant_token="")})
add("room_grant_claims", {"request": {}, "permission": "status",
                          "io": rg_io(decode_room_grant=script(raise_=rais("ValueError", "expired")))})
add("room_grant_claims", {"request": {}, "permission": "status",
                          "io": rg_io(room_grant_is_revoked=True)})
add("room_grant_claims", {"request": {}, "permission": "status",
                          "io": rg_io(peer_room_grant_is_current=False)})
add("room_grant_claims", {"request": {}, "permission": "status",
                          "io": rg_io(peer_room_grant_is_current=0)})
add("room_grant_claims", {"request": {}, "permission": "status",
                          "io": rg_io(room_grant_is_revoked=script(raise_=rais("OSError", "io")))})
add("room_grant_claims", {"request": {}, "permission": "status",
                          "io": rg_io(decode_room_grant=script(raise_=rais("SystemExit", "stop")))})
add("room_grant_claims", {"request": {}, "permission": "status",
                          "io": rg_io(open_persistence=script(raise_=rais("StoreUnavailable", "down")))})

# ── handle_room_member_capabilities ──
CAP_BASE = {"room_grant_claims": CLAIMS, "local_target": script(ret=["default", "inst1"]),
            "local_room_catalog": script(ret=[{"policy_digest": "d"}, {"catalog": 1}]),
            "web_json_response": {"resp": True}, "openai_error": {"env": True}}


def cap_io(**over):
    io = copy.deepcopy(CAP_BASE)
    io.update(over)
    return io


add("handle_room_member_capabilities", {"request": {}, "io": cap_io()})
add("handle_room_member_capabilities", {"request": {}, "io": cap_io(
    room_grant_claims=script(raise_=rais("ValueError", "bad grant")))})
add("handle_room_member_capabilities", {"request": {}, "io": cap_io(
    room_grant_claims=script(raise_=rais("RoomGrantReauthorizationRequired", "revoked")))})
add("handle_room_member_capabilities", {"request": {}, "io": cap_io(
    room_grant_claims={k: v for k, v in CLAIMS.items() if k != "member_id"})})
add("handle_room_member_capabilities", {"request": {}, "io": cap_io(
    local_target=script(raise_=rais("RuntimeError", "no profile")))})
add("handle_room_member_capabilities", {"request": {}, "io": cap_io(
    room_grant_claims=script(raise_=rais("SystemExit", "halt")))})
add("handle_room_member_capabilities", {"request": {}, "io": cap_io(
    local_room_catalog=script(raise_=rais("ValueError", "no catalog")))})

# ── handle_room_member_grant_revoke ──
REV_BASE = {"read_json_body": script(ret=[{}, None]), "room_grant_token": "tok",
            "import_decode_room_grant": None, "room_grant_secret": "sec",
            "decode_room_grant": CLAIMS2, "local_target": script(ret=["default", "inst1"]),
            "import_hosted_rooms": None, "import_persistence_assembly": None,
            "profile_scope": "default", "open_persistence": {"store": 1},
            "revoke_room_grant_scope": None, "web_json_response": {"resp": True},
            "openai_error": {"env": True}}


def rev_io(**over):
    io = copy.deepcopy(REV_BASE)
    io.update(over)
    return io


add("handle_room_member_grant_revoke", {"request": {}, "io": rev_io()})
add("handle_room_member_grant_revoke", {"request": {}, "io": rev_io(read_json_body=script(ret=[{"x": 1}, None]))})
add("handle_room_member_grant_revoke", {"request": {}, "io": rev_io(read_json_body=script(ret=[None, {"err": 1}]))})
add("handle_room_member_grant_revoke", {"request": {}, "io": rev_io(
    decode_room_grant=script(raise_=rais("ValueError", "unsigned")))})
add("handle_room_member_grant_revoke", {"request": {}, "io": rev_io(
    revoke_room_grant_scope=script(raise_=rais("OperationalError", "locked")))})
add("handle_room_member_grant_revoke", {"request": {}, "io": rev_io(
    revoke_room_grant_scope=script(raise_=rais("StoreUnavailable", "down")))})
add("handle_room_member_grant_revoke", {"request": {}, "io": rev_io(
    revoke_room_grant_scope=script(raise_=rais("OSError", "disk")))})
add("handle_room_member_grant_revoke", {"request": {}, "io": rev_io(
    revoke_room_grant_scope=script(raise_=rais("KeyError", "'k'")))})
add("handle_room_member_grant_revoke", {"request": {}, "io": rev_io(
    revoke_room_grant_scope=script(raise_=rais("RuntimeError", "other")))})
add("handle_room_member_grant_revoke", {"request": {}, "io": rev_io(
    revoke_room_grant_scope=script(raise_=rais("SystemExit", "halt")))})
add("handle_room_member_grant_revoke", {"request": {}, "io": rev_io(
    decode_room_grant={k: v for k, v in CLAIMS2.items() if k != "expires_at"})})

# ── handle_room_member_grant_refresh ──
REF_BASE = {"read_json_body": script(ret=[{"ttl_seconds": 120}, None]),
            "import_hosted_room_peer": None, "MAX_DISPATCH_GRANT_TTL_SECONDS": 86400,
            "import_hosted_room_execution_policy": None,
            "room_grant_claims": CLAIMS3, "local_target": script(ret=["default", "inst1"]),
            "time": 1000, "profile_scope_ctx": None,
            "execution_policy_mapping": {"policy_digest": "pd1"},
            "room_grant_secret": "sec", "uuid4_hex": "abc123",
            "issue_room_grant": "newtok", "web_json_response": {"resp": True},
            "openai_error": {"env": True}}


def ref_io(**over):
    io = copy.deepcopy(REF_BASE)
    io.update(over)
    return io


add("handle_room_member_grant_refresh", {"request": {}, "io": ref_io()})
add("handle_room_member_grant_refresh", {"request": {}, "io": ref_io(read_json_body=script(ret=[{}, None]))})
add("handle_room_member_grant_refresh", {"request": {}, "io": ref_io(
    read_json_body=script(ret=[{"ttl_seconds": 120, "extra": 1}, None]))})
add("handle_room_member_grant_refresh", {"request": {}, "io": ref_io(time=3000)})
add("handle_room_member_grant_refresh", {"request": {}, "io": ref_io(
    read_json_body=script(ret=[{"ttl_seconds": 0}, None]))})
add("handle_room_member_grant_refresh", {"request": {}, "io": ref_io(
    execution_policy_mapping={"policy_digest": "CHANGED"})})
add("handle_room_member_grant_refresh", {"request": {}, "io": ref_io(
    read_json_body=script(ret=[{"ttl_seconds": "120"}, None]))})
add("handle_room_member_grant_refresh", {"request": {}, "io": ref_io(
    read_json_body=script(ret=[{"ttl_seconds": "bad"}, None]))})
add("handle_room_member_grant_refresh", {"request": {}, "io": ref_io(
    room_grant_claims={k: v for k, v in CLAIMS3.items() if k != "permissions"})})
add("handle_room_member_grant_refresh", {"request": {}, "io": ref_io(
    time=script(raise_=rais("SystemExit", "halt")))})
add("handle_room_member_grant_refresh", {"request": {}, "io": ref_io(
    issue_room_grant=script(raise_=rais("ValueError", "mint failed")))})

# ── handle_room_member_invitation ──
INV_BASE = {"check_auth": None, "read_json_body": script(ret=[BODY_INV, None]),
            "local_target": script(ret=["default", "inst1"]),
            "issue_invitation": {"grant": "g", "target_profile": "default",
                                 "catalog": {}, "expires_at": 1, "status_expires_at": 2},
            "web_json_response": {"resp": True}, "openai_error": {"env": True}}


def inv_io(**over):
    io = copy.deepcopy(INV_BASE)
    io.update(over)
    return io


add("handle_room_member_invitation", {"request": {}, "self": {}, "io": inv_io()})
add("handle_room_member_invitation", {"request": {}, "self": {}, "io": inv_io(check_auth={"denied": True})})
add("handle_room_member_invitation", {"request": {}, "self": {}, "io": inv_io(read_json_body=script(ret=[None, {"err": 1}]))})
add("handle_room_member_invitation", {"request": {}, "self": {}, "io": inv_io(
    read_json_body=script(ret=[{k: v for k, v in BODY_INV.items() if k != "member_id"}, None]))})
add("handle_room_member_invitation", {"request": {}, "self": {}, "io": inv_io(
    read_json_body=script(ret=[dict(BODY_INV, surprise=1), None]))})
add("handle_room_member_invitation", {"request": {}, "self": {}, "io": inv_io(
    local_target=script(raise_=rais("ValueError", "not local")))})
add("handle_room_member_invitation", {"request": {}, "self": {}, "io": inv_io(
    issue_invitation=script(raise_=rais("StoreUnavailable", "down")))})
add("handle_room_member_invitation", {"request": {}, "self": {}, "io": inv_io(
    issue_invitation=script(raise_=rais("RuntimeError", "boom")))})
add("handle_room_member_invitation", {"request": {}, "self": {}, "io": inv_io(
    issue_invitation=script(raise_=rais("KeyError", "'k'")))})
add("handle_room_member_invitation", {"request": {}, "self": {}, "io": inv_io(
    issue_invitation=script(raise_=rais("SystemExit", "halt")))})

# ── validate_member_target (pure) ──
PEER_TARGET = {"kind": "peer", "peer_id": "p1", "installation_id": "i1",
               "profile": "default", "capability_digest": "a" * 64}
add("validate_member_target", {"value": None, "profile": "default",
                               "known_profiles": ["default"], "index": 0})
add("validate_member_target", {"value": None, "profile": "other",
                               "known_profiles": ["default"], "index": 1})
add("validate_member_target", {"value": 5, "profile": "default",
                               "known_profiles": ["default"], "index": 0})
add("validate_member_target", {"value": {"kind": "local", "profile": "default"},
                               "profile": "default", "known_profiles": ["default"], "index": 0})
add("validate_member_target", {"value": {"kind": "local", "profile": "other"},
                               "profile": "default", "known_profiles": ["default"], "index": 2})
add("validate_member_target", {"value": PEER_TARGET, "profile": "default",
                               "known_profiles": ["default"], "index": 0})
add("validate_member_target", {"value": dict(PEER_TARGET, capability_digest="xyz"),
                               "profile": "default", "known_profiles": ["default"], "index": 0})
add("validate_member_target", {"value": {"kind": "bogus"}, "profile": "default",
                               "known_profiles": ["default"], "index": 0})
add("validate_member_target", {"value": {"kind": "local"}, "profile": "default",
                               "known_profiles": ["default"], "index": 3})
add("validate_member_target", {"value": {"kind": "local", "profile": "default", "x": 1},
                               "profile": "default", "known_profiles": ["default"], "index": 0})

# ── resolve_mentions (pure) ──
add("resolve_mentions", {"texts": ["hi @alice"], "members": [M1, M2], "default_all": True})
add("resolve_mentions", {"texts": ["@bob @alice"], "members": [M1, M2], "default_all": True})
add("resolve_mentions", {"texts": ["@all"], "members": [M1, M2], "default_all": False})
add("resolve_mentions", {"texts": ["nothing here"], "members": [M1, M2], "default_all": True})
add("resolve_mentions", {"texts": ["nothing here"], "members": [M1, M2], "default_all": False})
add("resolve_mentions", {"texts": ["@ALICE"], "members": [M1, M2], "default_all": False})
add("resolve_mentions", {"texts": ["@unknown"], "members": [M1, M2], "default_all": False})
add("resolve_mentions", {"texts": [None, 5, "@bob"], "members": [M1, M2], "default_all": False})
add("resolve_mentions", {"texts": ["@everyone"], "members": [M1, M2], "default_all": False})
add("resolve_mentions", {"texts": ["@alice"], "members": [], "default_all": True})

# ── unaddressed_member_mentions ──
def mm(seq, eid, mid, text):
    return ev(seq, eid, "message.member",
              {"thread_id": "th1", "member_id": mid, "text": text})


add("unaddressed_member_mentions", {"messages": [mm(1, "e1", "m1", "@bob")], "room": ROOM})
add("unaddressed_member_mentions", {"messages": [mm(1, "e1", "m1", "@bob"),
                                                 mm(2, "e2", "m2", "hi")], "room": ROOM})
add("unaddressed_member_mentions", {"messages": [mm(1, "e1", "m1", "@bob"),
                                                 mm(2, "e2", "m2", "hi"),
                                                 mm(3, "e3", "m2", "@alice")], "room": ROOM})
add("unaddressed_member_mentions", {"messages": [ev(1, "u1", "message.user",
                                                    {"thread_id": "th1", "text": "@bob"})],
                                    "room": ROOM})
add("unaddressed_member_mentions", {"messages": [mm(1, "e1", "m1", "@bob"),
                                                 mm(3, "e3", "m2", "mid"),
                                                 mm(5, "e5", "m1", "@bob again")],
                                    "room": ROOM})

# ── member_by_id ──
add("member_by_id", {"room": ROOM, "member_id": "m1"})
add("member_by_id", {"room": ROOM, "member_id": "m2"})
add("member_by_id", {"room": ROOM, "member_id": "nope"})
add("member_by_id", {"room": ROOM, "member_id": "  m1  "})
add("member_by_id", {"room": ROOM, "member_id": 5})
add("member_by_id", {"room": ROOM, "member_id": None})

# ── derive_member_watermarks ──
def dw_io(events):
    return {"validate_room": ROOM, "validated_events": script(ret=events)}


TERM_OK = ev(3, "t1", "turn.settled",
             {"task_id": "task1", "thread_id": "th1", "member_id": "m1",
              "seen_through_seq": 5, "passed": True, "message_event_id": None,
              "discussion_event_id": "u1"})
add("derive_member_watermarks", {"room_value": ROOM, "events": [], "local_profiles": ["default"],
                                 "io": dw_io([])})
add("derive_member_watermarks", {"room_value": ROOM, "events": [TERM_OK],
                                 "local_profiles": ["default"], "io": dw_io([TERM_OK])})
NOTPASSED = ev(3, "t1", "turn.settled",
               {"task_id": "task1", "thread_id": "th1", "member_id": "m1",
                "seen_through_seq": 1, "passed": False, "message_event_id": "mm1",
                "discussion_event_id": "u1"})
MSG = ev(2, "mm1", "message.member",
         {"thread_id": "th1", "member_id": "m1", "task_id": "task1", "text": "a"})
add("derive_member_watermarks", {"room_value": ROOM, "events": [USER1, MSG, NOTPASSED],
                                 "local_profiles": ["default"],
                                 "io": dw_io([USER1, MSG, NOTPASSED])})
DUP = ev(4, "t2", "turn.failed",
         {"task_id": "task1", "thread_id": "th1", "member_id": "m1",
          "seen_through_seq": 6, "error": "x"})
add("derive_member_watermarks", {"room_value": ROOM, "events": [TERM_OK, DUP],
                                 "local_profiles": ["default"], "io": dw_io([TERM_OK, DUP])})
DEF1 = ev(3, "d1", "turn.deferred",
          {"task_id": "task1", "thread_id": "th1", "member_id": "m1",
           "seen_through_seq": 5, "execution_generation": 1, "reason": "r"})
DEF2 = ev(4, "d2", "turn.deferred",
          {"task_id": "task1", "thread_id": "th1", "member_id": "m1",
           "seen_through_seq": 6, "execution_generation": 2, "reason": "r"})
add("derive_member_watermarks", {"room_value": ROOM, "events": [DEF1, DEF2],
                                 "local_profiles": ["default"], "io": dw_io([DEF1, DEF2])})
DEF_BAD = ev(4, "d2", "turn.deferred",
             {"task_id": "task1", "thread_id": "th1", "member_id": "m1",
              "seen_through_seq": 6, "execution_generation": 1, "reason": "r"})
add("derive_member_watermarks", {"room_value": ROOM, "events": [DEF1, DEF_BAD],
                                 "local_profiles": ["default"], "io": dw_io([DEF1, DEF_BAD])})
# fail-closed: validate_room script missing (live has no try).
add("derive_member_watermarks", {"room_value": ROOM, "events": [], "local_profiles": ["default"],
                                 "io": {"validated_events": script(ret=[])}})

# ── make_task_plan ──
MT_BASE = {"room": ROOM, "discussion_event": USER1, "member": M1,
           "member_index": 0, "round_index": 0, "seen_through_seq": 1,
           "prompt": "hello", "input_context": None, "attachments": None,
           "lead_routing": False}
add("make_task_plan", dict(MT_BASE, io={}))
add("make_task_plan", dict(MT_BASE, lead_routing=True, io={}))
add("make_task_plan", dict(MT_BASE, input_context={"watermark": 0, "event_seqs": [1]}, io={}))
ATT = [{"event_id": "u1", "name": "f.txt"}]
ATT_VAL = [{"event_id": "u1", "name": "f.txt", "size": 3}]
add("make_task_plan", dict(MT_BASE, attachments=ATT,
                           io={"validate_bound_task_manifest": script(ret=ATT_VAL)}))
add("make_task_plan", dict(MT_BASE, attachments=ATT,
                           io={"validate_bound_task_manifest":
                               script(raise_=rais("DriverValidationError", "attachments must be a non-empty list"))}))

# ── plan_next_task ──
PN_BASE = {"validate_room": ROOM,
           "validate_context": {"purpose": "", "notes": "", "routing": "everyone",
                                "lead_member_id": ""},
           "validated_events": script(ret=[USER1]), "router.continued_through": {},
           "router.continue_count": 0, "router.routed_responders": None,
           "lead_round": None, "router.resume_round": 0,
           "scheduled_post_kind": None,
           "bounded_task_delta": script(ret=[[USER1], []]), "build_prompt": "PROMPT"}


def pn_io(**over):
    io = copy.deepcopy(PN_BASE)
    io.update(over)
    return io


def pn_in(**over):
    base = {"room_value": ROOM, "events": [USER1], "local_profiles": ["default"],
            "initial_watermarks": None, "freeze_input_context": False,
            "limits": None, "room_context": None}
    base.update(over)
    return base


add("plan_next_task", pn_in(io=pn_io(validated_events=script(ret=[]))))
add("plan_next_task", pn_in(io=pn_io()))
add("plan_next_task", pn_in(limits={"max_rounds": 3, "max_messages": 1},
                            events=[USER1, MM1, SETTLED1],
                            io=pn_io(validated_events=script(ret=[USER1, MM1, SETTLED1]))))
add("plan_next_task", pn_in(io=pn_io(**{"router.routed_responders": {"__sentinel__": "ROUTE_PENDING"}})))
add("plan_next_task", pn_in(io=pn_io(lead_round={
    "task": TASK_LEAD, "routing_task_id": TASK_LEAD["identity"]["task_id"],
    "responders": [], "lead": M1})))
add("plan_next_task", pn_in(io=pn_io(bounded_task_delta=script(ret=[[], []]))))
add("plan_next_task", pn_in(freeze_input_context=True,
                            io=pn_io(validate_task_input={"watermark": 0, "event_seqs": [1]})))
TERM_OTHER = ev(5, "to1", "turn.settled",
                {"task_id": "other", "thread_id": "th1", "member_id": "m1",
                 "discussion_event_id": "u1", "round_index": 0, "member_index": 0,
                 "turn_id": "x", "seen_through_seq": 3,
                 "message_event_id": "mm1", "passed": False})
TERM_OTHER2 = ev(6, "to2", "turn.settled",
                 {"task_id": "other2", "thread_id": "th1", "member_id": "m2",
                  "discussion_event_id": "u1", "round_index": 0, "member_index": 1,
                  "turn_id": "y", "seen_through_seq": 3,
                  "message_event_id": None, "passed": True})
add("plan_next_task", pn_in(limits={"max_rounds": 1, "max_messages": 10},
                            events=[USER1, MM1, SETTLED1, TERM_OTHER, TERM_OTHER2],
                            io=pn_io(validated_events=script(ret=[USER1, MM1, SETTLED1, TERM_OTHER, TERM_OTHER2]))))
# fail-closed: a required mid-flow seam has no script (RuntimeError propagates;
# live plan_next_task only catches ValueError).
_pn_fc = pn_io()
del _pn_fc["router.continue_count"]
add("plan_next_task", pn_in(io=_pn_fc))

# ── reconstruct_task_plan ──
RC_BASE = {"validate_room": ROOM, "validated_events": script(ret=[USER1])}


def rc_in(task, **over):
    io = copy.deepcopy(RC_BASE)
    io.update(over.pop("io", {}))
    base = {"room_value": ROOM, "events": [USER1], "task": task,
            "local_profiles": ["default"], "io": io}
    base.update(over)
    return base


add("reconstruct_task_plan", rc_in(TASK1))
add("reconstruct_task_plan", rc_in(dict(TASK1, identity=dict(TASK1["identity"], turn_id="bogus"))))
add("reconstruct_task_plan", rc_in(dict(TASK1, payload=dict(TASK1["payload"], source_event_seq=9))))
add("reconstruct_task_plan", rc_in(TASK1, io={"validated_events": script(ret=[])}))
add("reconstruct_task_plan", rc_in(dict(TASK1, payload=dict(TASK1["payload"], extra_field=1))))
add("reconstruct_task_plan", rc_in(dict(TASK1, identity=None)))
add("reconstruct_task_plan", rc_in(dict(TASK1, payload=dict(TASK1["payload"], prompt="  "))))
add("reconstruct_task_plan", rc_in(dict(TASK1, payload=dict(TASK1["payload"], target_member_id="m2"))))
add("reconstruct_task_plan", rc_in(TASK_IC, io={"validate_task_input": {"watermark": 0, "event_seqs": [1]}}))
# fail-closed: validated_events script missing (live only catches ValueError).
_rc_fc = copy.deepcopy(RC_BASE)
del _rc_fc["validated_events"]
add("reconstruct_task_plan", rc_in(TASK1, io=_rc_fc))

# ── plan_publication ──
PP_BASE = {"validate_room": ROOM, "validated_events": script(ret=[USER1]),
           "lead_routing.is_routing_task": False}


def pp_in(**over):
    io = copy.deepcopy(PP_BASE)
    io.update(over.pop("io", {}))
    base = {"room_value": ROOM, "events": [USER1], "task": TASK1,
            "status": "settled", "result": {"text": "hello"},
            "execution_generation": None, "local_profiles": ["default"], "io": io}
    base.update(over)
    return base


add("plan_publication", pp_in())
add("plan_publication", pp_in(result={"text": "(pass)"}))
add("plan_publication", pp_in(status="failed", result={"error": "boom", "reason_code": "timeout"},
                              io={"all_failure_reasons": script(ret=["timeout", "rate_limited"]),
                                  "import_classify_agent_error": None}))
add("plan_publication", pp_in(status="failed", result={"error": "boom"},
                              io={"all_failure_reasons": script(ret=["timeout"]),
                                  "import_classify_agent_error": None,
                                  "classify_agent_error": "rate_limited"}))
add("plan_publication", pp_in(status="cancelled", events=[USER1, USER2],
                              io={"validated_events": script(ret=[USER1, USER2])}))
add("plan_publication", pp_in(status="deferred", execution_generation=2,
                              result={"reason": "member busy"}))
add("plan_publication", pp_in(status="deferred", execution_generation=0))
add("plan_publication", pp_in(task=dict(TASK1, identity=dict(TASK1["identity"], room_id="other"))))
AE = {"event_id": "ra1", "kind": "room.assignment", "actor": {"kind": "gateway", "id": "gw1"},
      "payload": {"lead_member_id": "m1"}, "authority_gateway_id": "gw1", "authority_epoch": 7}
add("plan_publication", pp_in(io={"lead_routing.is_routing_task": True,
                                  "lead_routing.assignment_effect": script(ret=[AE])}))
# fail-closed: is_routing_task script missing (live plan_publication has no try).
_pp_fc = copy.deepcopy(PP_BASE)
del _pp_fc["lead_routing.is_routing_task"]
add("plan_publication", pp_in(io=_pp_fc))

# ── write ──
out = TESTS / "fixtures" / "rest_revive4.jsonl"
out.parent.mkdir(parents=True, exist_ok=True)
with open(out, "w", encoding="utf-8", newline="") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")
counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
print(f"wrote {len(CASES)} cases to {out}")
for k in sorted(counts):
    print(f"  {k}: {counts[k]}")
assert all(v >= 5 for v in counts.values()), "every function needs >=5 cases"
assert len(counts) == 20
