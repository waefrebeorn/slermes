#!/usr/bin/env python3
"""Generate tests/fixtures/hosted_rooms_flow.jsonl for the hosted_rooms_flow
decision-sequence oracle (t_port_hosted_rooms_flow.c / oracle_hosted_rooms_flow.py).

Case shape: {"func": <live name>, "in": {"args": {...}, "io": {...}}}.
io scripts: plain value (reused), LIST consumed per call (conn.execute scripts
are queues of ROWS lists), or {"__script__": true, "ret": v | "raise": spec}.
List-valued verb returns are wrapped in __script__ (bare arrays are queues).

Run: python tests/gen_hosted_rooms_flow_cases.py
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import oracle_hosted_rooms_flow as O  # noqa: E402  (extraction runs at import)

OUT = os.path.join(HERE, "fixtures", "hosted_rooms_flow.jsonl")
NS = O.NS
BACKFILL = NS["_EVENT_BYTES_BACKFILL"].format(where="1")
INSERT = NS["_INSERT_EVENT"]
SUM = NS["_SUM_EVENT_BYTES"]
LEGACY_ACTOR_DEFAULT = "'" + NS["_LEGACY_ACTOR_JSON"] + "'"

CASES = []


def case(func, args, io=None):
    CASES.append({"func": func, "in": {"args": args, "io": io or {}}})


def raises(t, msg):
    return {"__script__": True, "raise": {"type": t, "message": msg}}


def ret(v):
    return {"__script__": True, "ret": v}


def pragma_rows(cols):
    return [[i, c, "TEXT", 0, None, 0] for i, c in enumerate(cols)]


ROOMS_COLS = ["room_id", "name", "members_json", "authority_gateway_id", "authority_epoch",
              "next_seq", "event_bytes", "revision", "created_at", "updated_at", "disbanded_at"]
EVENTS_COLS = ["room_id", "seq", "event_id", "kind", "actor_json", "authority_epoch",
               "payload_json", "created_at"]
RROOM = {"next_seq": 10, "event_bytes": 100}
EID, KIND = "e1", "message.user"
AJ = '{"id":"u1","kind":"user"}'
PJ = '{"text":"hi"}'
ADD = len((EID + KIND + AJ + PJ).encode("utf-8"))
IDENT = {"room_id": "r1", "home_install_id": "h1", "authority_gateway_id": "g1",
         "authority_epoch": 2, "member_id": "m1", "target_install_id": "t1",
         "target_profile": "p1", "task_id": "task1", "execution_generation": 1}
GCLAIMS = {"room_id": "r1", "home_install_id": "h1", "authority_gateway_id": "g1",
           "authority_epoch": 2, "member_id": "m1", "target_install_id": "t1",
           "target_profile": "p1", "issued_at": 1.5}
RCLAIMS = {"room_id": "r1", "member_id": "m1", "target_profile": "p1",
           "authority_gateway_id": "g1", "authority_epoch": 2,
           "home_install_id": "h1", "target_install_id": "t1", "issued_at": 9.5}
VERB = "hosted_rooms.create_room"

# ── create_room ──
case("create_room", {"placement": "shared", "room_id": "r1", "name": "Room One",
                     "members": [{"id": "m1"}], "authority_gateway_id": "g1",
                     "now": 1700000000, "owner": None},
     {VERB: ret({"room_id": "r1", "revision": 1})})
case("create_room", {"placement": "profile", "room_id": "r1", "name": "Room One",
                     "members": [], "authority_gateway_id": "g1", "now": 1700000000,
                     "owner": {"epoch": 1, "actor": "a1", "rule": None}},
     {VERB: ret({"room_id": "r1"})})
case("create_room", {"placement": "shared", "room_id": "r1", "name": "Room One",
                     "members": [{"id": "m1"}], "authority_gateway_id": "g1", "now": None},
     {"clock": 1700000000.5, VERB: ret({"room_id": "r1"})})
case("create_room", {"placement": "shared", "room_id": "-r1", "name": "n",
                     "members": [], "authority_gateway_id": "g1", "now": 1})
case("create_room", {"placement": "shared", "room_id": 5, "name": "n",
                     "members": [], "authority_gateway_id": "g1", "now": 1})
case("create_room", {"placement": "shared", "room_id": "r1", "name": "   ",
                     "members": [], "authority_gateway_id": "g1", "now": 1})
case("create_room", {"placement": "shared", "room_id": "r1", "name": "  Café Room!  ",
                     "members": [], "authority_gateway_id": "g1", "now": 1},
     {VERB: ret({"room_id": "r1"})})
case("create_room", {"placement": "shared", "room_id": "r1", "name": "n",
                     "members": {}, "authority_gateway_id": "g1", "now": 1})
case("create_room", {"placement": "shared", "room_id": "r1", "name": "n",
                     "members": [1], "authority_gateway_id": "g1", "now": 1})
case("create_room", {"placement": "shared", "room_id": "r1", "name": "n",
                     "members": [{} for _ in range(129)], "authority_gateway_id": "g1", "now": 1})
case("create_room", {"placement": "shared", "room_id": "r1", "name": "n",
                     "members": [{"id": "m1", "blob": "x" * 131100}],
                     "authority_gateway_id": "g1", "now": 1})
case("create_room", {"placement": "shared", "room_id": "r1", "name": "n",
                     "members": [], "authority_gateway_id": "", "now": 1})
case("create_room", {"placement": "shared", "room_id": "r1", "name": "n",
                     "members": [], "authority_gateway_id": "g1", "now": 1},
     {VERB: raises("HostedRoomError", "store refused")})

# ── list_rooms ──
LV = "hosted_rooms.list_rooms"
case("list_rooms", {"placement": "profile"}, {LV: ret([])})
case("list_rooms", {"placement": "shared", "include_disbanded": True, "limit": 10, "offset": 5},
     {LV: ret([{"room_id": "r1"}])})
case("list_rooms", {"placement": "profile", "limit": 0})
case("list_rooms", {"placement": "profile", "limit": 501})
case("list_rooms", {"placement": "profile", "limit": True})
case("list_rooms", {"placement": "profile", "limit": "500"})
case("list_rooms", {"placement": "profile", "offset": -1})
case("list_rooms", {"placement": "profile", "offset": 1.5})
case("list_rooms", {"placement": "profile", "limit": 500, "offset": 0}, {LV: ret([])})

# ── room_has_peer_obligations ──
OV = "hosted_room_peers.room_has_peer_obligations"
case("room_has_peer_obligations", {"placement": "shared", "room_id": "r1", "now": 1700000000},
     {OV: True})
case("room_has_peer_obligations", {"placement": "shared", "room_id": "r1", "now": None},
     {"clock": 1700000000, OV: False})
case("room_has_peer_obligations", {"placement": "shared", "room_id": "bad id", "now": 1})
case("room_has_peer_obligations", {"placement": "shared", "room_id": None, "now": 1})
case("room_has_peer_obligations", {"placement": "shared", "room_id": "r1", "now": 1},
     {OV: raises("RuntimeError", "store busy")})
case("room_has_peer_obligations", {"placement": "shared", "room_id": " r1 ", "now": 1},
     {OV: True})

# ── retire_room_link_records ──
RV = "hosted_room_peers.retire_room_link_records"
case("retire_room_link_records", {"placement": "shared", "room_id": "r1", "now": 1700000000,
                                  "revoked_digests": None}, {RV: 2})
case("retire_room_link_records", {"placement": "shared", "room_id": "r1", "now": 1700000000,
                                  "revoked_digests": ["d2", "d1", "d2"]}, {RV: 2})
case("retire_room_link_records", {"placement": "shared", "room_id": "r1", "now": 1700000000,
                                  "revoked_digests": []}, {RV: 0})
case("retire_room_link_records", {"placement": "shared", "room_id": "r1", "now": None,
                                  "revoked_digests": ["d1"]}, {"clock": 1700000000, RV: 1})
case("retire_room_link_records", {"placement": "shared", "room_id": "r1", "now": None,
                                  "revoked_digests": 5}, {"clock": 1700000000})
case("retire_room_link_records", {"placement": "shared", "room_id": "r1", "now": 1700000000,
                                  "revoked_digests": "ab"}, {RV: 2})
case("retire_room_link_records", {"placement": "shared", "room_id": "r1", "now": 1700000000,
                                  "owe_routes": True}, {RV: 1})
case("retire_room_link_records", {"placement": "shared", "room_id": "r1", "now": 1700000000,
                                  "revoked_digests": [["x"]]})

# ── due_room_grant_revokes ──
DV = "hosted_room_peers.due_room_grant_revokes"
case("due_room_grant_revokes", {"placement": "shared", "limit": 10, "now": 1700000000},
     {DV: ret([{"room_id": "r1", "grant_digest": "d1"}])})
case("due_room_grant_revokes", {"placement": "shared", "limit": "10", "now": 1700000000},
     {DV: ret([])})
case("due_room_grant_revokes", {"placement": "shared", "limit": 2.9, "now": 1700000000},
     {DV: ret([])})
case("due_room_grant_revokes", {"placement": "shared", "limit": None, "now": 1700000000})
case("due_room_grant_revokes", {"placement": "shared", "limit": 10, "now": 1700000000},
     {DV: ret(None)})
case("due_room_grant_revokes", {"placement": "shared", "limit": 10, "now": 1700000000},
     {DV: ret([5])})
case("due_room_grant_revokes", {"placement": "shared", "limit": 10, "now": 1700000000},
     {DV: ret([["a", 1]])})
case("due_room_grant_revokes", {"placement": "shared", "limit": 10, "now": None},
     {"clock": 1700000000, DV: ret([])})
case("due_room_grant_revokes", {"placement": "shared", "limit": True, "now": 1700000000},
     {DV: ret([])})

# ── settle_room_grant_revoke ──
SV = "hosted_room_peers.settle_room_grant_revoke"
SARGS = {"placement": "shared", "room_id": "r1", "member_id": "m1", "grant_digest": "d1"}
case("settle_room_grant_revoke", dict(SARGS, retry_at=None), {SV: ret(None)})
case("settle_room_grant_revoke", dict(SARGS, retry_at=5), {SV: ret(None)})
case("settle_room_grant_revoke", dict(SARGS, retry_at="12.5"), {SV: ret(None)})
case("settle_room_grant_revoke", dict(SARGS, retry_at="abc"))
case("settle_room_grant_revoke", dict(SARGS, retry_at=[]))
case("settle_room_grant_revoke", dict(SARGS, retry_at=True), {SV: ret(None)})
case("settle_room_grant_revoke", dict(SARGS, retry_at=1), {SV: raises("HostedRoomError", "gone")})
case("settle_room_grant_revoke", dict(SARGS, retry_at="1_0.5"), {SV: ret(None)})

# ── drop_remote_run_receipt ──
DV2 = "hosted_room_peers.drop_remote_run_receipt"
case("drop_remote_run_receipt", {"placement": "shared", "record": dict(IDENT), "run_id": "run1"},
     {DV2: True})
no_gen = dict(IDENT)
del no_gen["execution_generation"]
case("drop_remote_run_receipt", {"placement": "shared", "record": no_gen, "run_id": "run1"})
no_rid = dict(IDENT)
del no_rid["room_id"]
case("drop_remote_run_receipt", {"placement": "shared", "record": no_rid, "run_id": "run1"})
case("drop_remote_run_receipt", {"placement": "shared", "record": None, "run_id": "run1"})
case("drop_remote_run_receipt", {"placement": "shared", "record": [1], "run_id": "run1"})
case("drop_remote_run_receipt", {"placement": "shared", "record": dict(IDENT), "run_id": "run1"},
     {DV2: False})
case("drop_remote_run_receipt", {"placement": "shared", "record": dict(IDENT, extra="x"),
                                 "run_id": "run1"}, {DV2: True})

# ── _typed ──
case("_typed", {"type": "RemoteRunIdentity", "record": dict(IDENT)})
case("_typed", {"type": "RoomGrantClaims", "record": dict(GCLAIMS)})
case("_typed", {"type": "RemoteRunReceipt", "record": dict(IDENT, run_id="run1", session_id="s1")})
case("_typed", {"type": "RoomLinkRecord",
                "record": {"room_id": "r1", "member_id": "m1", "target_url": "https://x",
                           "target_profile": "p1", "grant": "g", "catalog_json": "{}",
                           "cancellation_scope_id": "c1", "trace_id": "t1",
                           "transport_security": "tls", "status": "ready", "updated_at": 1.0}})
no_mid = dict(IDENT)
del no_mid["member_id"]
case("_typed", {"type": "RemoteRunIdentity", "record": no_mid})
case("_typed", {"type": "RemoteRunIdentity", "record": None})
case("_typed", {"type": "RemoteRunIdentity", "record": dict(IDENT, extra="x")})

# ── _grant_claims ──
case("_grant_claims", {"claims": dict(GCLAIMS)})
case("_grant_claims", {"claims": {}})
case("_grant_claims", {"claims": {"room_id": 0, "home_install_id": "", "authority_gateway_id": None,
                                  "member_id": False, "target_install_id": 0, "target_profile": ""}})
case("_grant_claims", {"claims": dict(GCLAIMS, authority_epoch="3", issued_at="1.5")})
case("_grant_claims", {"claims": dict(GCLAIMS, authority_epoch=2.9)})
case("_grant_claims", {"claims": dict(GCLAIMS, issued_at=True)})
case("_grant_claims", {"claims": dict(GCLAIMS, room_id=5, target_profile={"a": 1})})
case("_grant_claims", {"claims": None})
case("_grant_claims", {"claims": dict(GCLAIMS, authority_epoch="bad")})
case("_grant_claims", {"claims": dict(GCLAIMS, room_id=[1, 2])})

# ── _reservation_claims ──
case("_reservation_claims", {"claims": dict(RCLAIMS)})
case("_reservation_claims", {"claims": dict(RCLAIMS, room_id="-x")})
case("_reservation_claims", {"claims": dict(RCLAIMS, member_id="")})
case("_reservation_claims", {"claims": dict(RCLAIMS, target_profile="a b")})
case("_reservation_claims", {"claims": dict(RCLAIMS, authority_gateway_id=None)})
case("_reservation_claims", {"claims": dict(RCLAIMS, authority_epoch=0)})
case("_reservation_claims", {"claims": dict(RCLAIMS, authority_epoch="2")})
case("_reservation_claims", {"claims": dict(RCLAIMS, authority_epoch=None)})
case("_reservation_claims", {"claims": dict(RCLAIMS, authority_epoch=-1)})
case("_reservation_claims", {"claims": dict(RCLAIMS, room_id="-x", member_id="")})
case("_reservation_claims", {"claims": dict(RCLAIMS, authority_epoch=1.9)})

# ── _reservation_grant ──
case("_reservation_grant", {"claims": dict(RCLAIMS)})
case("_reservation_grant", {"claims": dict(RCLAIMS, home_install_id=0)})
case("_reservation_grant", {"claims": dict(RCLAIMS, target_install_id=7)})
case("_reservation_grant", {"claims": dict(RCLAIMS, member_id="bad id")})
case("_reservation_grant", {"claims": dict(RCLAIMS, authority_epoch="3")})
case("_reservation_grant", {"claims": dict(RCLAIMS, issued_at=None)})

# ── default_db_path ──
case("default_db_path", {"home": "/h/profiles/p1"})
case("default_db_path", {"home": "/h"})
case("default_db_path", {"home": None}, {"get_hermes_home": "/h/profiles/p2"})
case("default_db_path", {"home": "rel/profiles/p1"})
case("default_db_path", {"home": "profiles/p1"})
case("default_db_path", {"home": ""})
case("default_db_path", {"home": "/profiles"})
case("default_db_path", {"home": "x/profiles/"})
case("default_db_path", {"home": 5})
case("default_db_path", {"home": None}, {"get_hermes_home": raises("RuntimeError", "no home")})
case("default_db_path", {"home": "/h/p1/"})

# ── _room_row ──
RR_SQL = "SELECT * FROM hosted_rooms WHERE room_id=?"
case("_room_row", {"sql": RR_SQL, "params": ["r1"], "room_id": "r1"},
     {"conn.execute": [[{"room_id": "r1", "name": "n"}]]})
case("_room_row", {"sql": RR_SQL, "params": ["r1"], "room_id": "r1"},
     {"conn.execute": [[], [1]]})
case("_room_row", {"sql": RR_SQL, "params": ["r1"], "room_id": "r1"},
     {"conn.execute": [[], [], [1]]})
case("_room_row", {"sql": RR_SQL, "params": ["r1"], "room_id": "r1"},
     {"conn.execute": [[], [], []]})
case("_room_row", {"sql": RR_SQL, "params": ["r1"], "room_id": "r1"},
     {"conn.execute": [[], [1], [1]]})
case("_room_row", {"sql": RR_SQL, "params": ["r1"], "room_id": "r1"},
     {"conn.execute": [raises("RuntimeError", "locked")]})
case("_room_row", {"sql": RR_SQL, "params": ["r9", 2], "room_id": "r9"},
     {"conn.execute": [[{"room_id": "r9"}]]})

# ── _is_retired ──
case("_is_retired", {"room_id": "r1"}, {"conn.execute": [[1]]})
case("_is_retired", {"room_id": "r1"}, {"conn.execute": [[]]})
case("_is_retired", {"room_id": "r1"}, {"conn.execute": [raises("RuntimeError", "locked")]})
case("_is_retired", {"room_id": "any id!"}, {"conn.execute": [[1]]})
case("_is_retired", {"room_id": "r1"}, {"conn.execute": [ret(None)]})

# ── _require_authority ──
RA_ROOM = {"authority_gateway_id": "g1", "authority_epoch": 2}
case("_require_authority", {"room": dict(RA_ROOM), "gateway_id": "g1", "epoch": 2, "message": "fence"})
case("_require_authority", {"room": dict(RA_ROOM), "gateway_id": "g2", "epoch": 2, "message": "fence"})
case("_require_authority", {"room": dict(RA_ROOM), "gateway_id": "g1", "epoch": 3, "message": "fence"})
case("_require_authority", {"room": dict(RA_ROOM), "gateway_id": "g1", "epoch": "2", "message": "fence"})
case("_require_authority", {"room": dict(RA_ROOM, authority_epoch="2"), "gateway_id": "g1",
                           "epoch": 2, "message": "fence"})
case("_require_authority", {"room": dict(RA_ROOM, authority_epoch=2.0), "gateway_id": "g1",
                           "epoch": 2, "message": "fence"})
case("_require_authority", {"room": dict(RA_ROOM, authority_gateway_id=5), "gateway_id": "5",
                           "epoch": 2, "message": "fence"})
case("_require_authority", {"room": {"authority_gateway_id": "g1"}, "gateway_id": "g1",
                           "epoch": 2, "message": "fence"})
case("_require_authority", {"room": None, "gateway_id": "g1", "epoch": 2, "message": "fence"})
case("_require_authority", {"room": dict(RA_ROOM), "gateway_id": "g1", "epoch": 2.0, "message": "fence"})

# ── _migrate_legacy_columns ──
def without(cols, name):
    return [c for c in cols if c != name]


ALTER_AGID = "ALTER TABLE hosted_rooms ADD COLUMN authority_gateway_id TEXT NOT NULL DEFAULT 'legacy'"
ALTER_EPOCH = "ALTER TABLE hosted_rooms ADD COLUMN authority_epoch INTEGER NOT NULL DEFAULT 1"
ALTER_EBYTES = "ALTER TABLE hosted_rooms ADD COLUMN event_bytes INTEGER NOT NULL DEFAULT 0"
ALTER_ACTOR = ("ALTER TABLE hosted_room_events ADD COLUMN actor_json TEXT NOT NULL DEFAULT "
               + LEGACY_ACTOR_DEFAULT)
ALTER_EEPOCH = "ALTER TABLE hosted_room_events ADD COLUMN authority_epoch INTEGER"
case("_migrate_legacy_columns", {},
     {"conn.execute": [pragma_rows(ROOMS_COLS), pragma_rows(EVENTS_COLS)]})
case("_migrate_legacy_columns", {},
     {"conn.execute": [pragma_rows(without(ROOMS_COLS, "authority_gateway_id")),
                       [], pragma_rows(EVENTS_COLS)]})
case("_migrate_legacy_columns", {},
     {"conn.execute": [pragma_rows(without(ROOMS_COLS, "event_bytes")),
                       [], pragma_rows(EVENTS_COLS), []]})
case("_migrate_legacy_columns", {},
     {"conn.execute": [pragma_rows(ROOMS_COLS),
                       pragma_rows(without(EVENTS_COLS, "actor_json")), []]})
case("_migrate_legacy_columns", {},
     {"conn.execute": [pragma_rows(ROOMS_COLS),
                       pragma_rows(without(EVENTS_COLS, "authority_epoch")), []]})
case("_migrate_legacy_columns", {},
     {"conn.execute": [[], [], [], [], [], [], [], []]})
case("_migrate_legacy_columns", {},
     {"conn.execute": [raises("RuntimeError", "disk")]})
case("_migrate_legacy_columns", {},
     {"conn.execute": [pragma_rows(without(ROOMS_COLS, "authority_gateway_id")),
                       raises("RuntimeError", "disk")]})

# ── _prepare_event ──
PE_ARGS = {"room": dict(RROOM), "event_id": EID, "kind": KIND, "actor_json": AJ, "payload_json": PJ}
case("_prepare_event", dict(PE_ARGS), {"conn.execute": [[[1000]]]})
case("_prepare_event", dict(PE_ARGS, room={"next_seq": 50001, "event_bytes": 100}))
case("_prepare_event", dict(PE_ARGS, room={"next_seq": 50001, "event_bytes": 100},
                            allow_control=True), {"conn.execute": [[[1000]]]})
case("_prepare_event", dict(PE_ARGS, room={"next_seq": 10, "event_bytes": 268435456}))
case("_prepare_event", dict(PE_ARGS),
     {"conn.execute": [[[16777216]], [[0]]], "_prune_disbanded_rooms_locked": 0})
case("_prepare_event", dict(PE_ARGS),
     {"conn.execute": [[[16777216]], [[16777216]]], "_prune_disbanded_rooms_locked": 0})
case("_prepare_event", dict(PE_ARGS, allow_control=True),
     {"conn.execute": [[[16777216 + 1048576]], [[0]]], "_prune_disbanded_rooms_locked": 0})
case("_prepare_event", dict(PE_ARGS, event_id=5))
case("_prepare_event", dict(PE_ARGS, room={"event_bytes": 100}))
case("_prepare_event", dict(PE_ARGS), {"conn.execute": [[[16777216 - ADD]]]})
case("_prepare_event", dict(PE_ARGS), {"conn.execute": [[None]]})

# ── _insert_event ──
IE_ARGS = dict(PE_ARGS, room_id="r1", seq=7, epoch=3, now=1700000000.25)
case("_insert_event", dict(IE_ARGS), {"conn.execute": [[[1000]], []]})
case("_insert_event", dict(IE_ARGS, room={"next_seq": 50001, "event_bytes": 100}))
case("_insert_event", dict(IE_ARGS),
     {"conn.execute": [[[1000]], raises("RuntimeError", "disk full")]})
case("_insert_event", dict(IE_ARGS, room={"next_seq": 50001, "event_bytes": 100},
                           allow_control=True), {"conn.execute": [[[1000]], []]})
case("_insert_event", dict(IE_ARGS, seq=9, epoch=4, now=1700000001),
     {"conn.execute": [[[1000]], []]})
case("_insert_event", dict(IE_ARGS, payload_json='{"text":"café"}'),
     {"conn.execute": [[[1000]], []]})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    per = {}
    for c in CASES:
        per[c["func"]] = per.get(c["func"], 0) + 1
    short = [f for f in O.FUNCS if per.get(f, 0) < 5]
    if short:
        print(f"FATAL: funcs with <5 cases: {short}")
        sys.exit(2)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"wrote {OUT}: {len(CASES)} cases over {len(per)} funcs")
    for fn in O.FUNCS:
        print(f"  {fn}: {per[fn]}")


if __name__ == "__main__":
    main()