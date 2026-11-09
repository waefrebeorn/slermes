#!/usr/bin/env python3
"""Oracle for port_hosted_rooms.c -- drives the LIVE
gateway/hosted_rooms.py + hosted_rooms_common.py defs. The sqlite half is not
imported; the pure defs are ast-extracted with the module constants they read.
"""
import ast
import hashlib
import json
import re
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

ROOMS = r"C:\Users\eman5\hermes-agent\gateway\hosted_rooms.py"
COMMON = r"C:\Users\eman5\hermes-agent\gateway\hosted_rooms_common.py"


class HostedRoomError(Exception):
    pass


def extract():
    ns = {"re": re, "json": json, "hashlib": hashlib, "Any": object,
          "Mapping": dict, "sqlite3": None, "functools": None,
          "HostedRoomError": HostedRoomError, "partial": None}
    import functools as _ft
    ns["partial"] = _ft.partial
    ns["functools"] = _ft
    ns["contextlib"] = __import__("contextlib")
    for path in (COMMON, ROOMS):
        src = open(path, encoding="utf-8").read()
        for node in ast.parse(src).body:
            if isinstance(node, ast.Assign):
                try:
                    exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)
                except Exception:
                    pass
    # the pure defs
    for path, names in ((COMMON, ["bounded_int", "identifier", "canonical_json", "compact_json", "utf8_len", "exact_fields", "text"]),
                        (ROOMS, ["_claim_values", "_room_grant_scope_key",
                                 "_reservation_superseded", "_remote_run_identity",
                                 "_validate_members", "user_event_id"])):
        src = open(path, encoding="utf-8").read()
        for node in ast.parse(src).body:
            if isinstance(node, ast.FunctionDef) and node.name in names:
                exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)
    # hosted_rooms.py builds its vocabulary from functools.partial aliases at
    # import time (_room_id, _event_id, _actor_json, _payload_json,
    # _validate_identifier). The generic Assign loop above fails on them because
    # the names they bind are only defined once the defs are extracted, so bind
    # them explicitly here.
    # the real module constant (hosted_rooms.py:63)
    ns["_REMOTE_RUN_IDENTITY_COLUMNS"] = (
        "room_id", "home_install_id", "authority_gateway_id", "authority_epoch",
        "member_id", "target_install_id", "target_profile", "task_id",
        "execution_generation")
    ns["_canonical_json"] = ns["partial"](ns["canonical_json"], error=HostedRoomError,
                                          ensure_ascii=False)
    ns["_validate_identifier"] = ns["partial"](ns["identifier"], error=HostedRoomError)
    ns["_event_id"] = ns["partial"](ns["_validate_identifier"], label="event_id", max_chars=128)
    ns["_actor_json"] = ns["partial"](ns["_canonical_json"], label="actor", max_bytes=4 * 1024)
    ns["_payload_json"] = ns["partial"](ns["_canonical_json"], label="payload",
                                      max_bytes=256 * 1024)
    return ns


def call(fn, *a, **kw):
    try:
        return True, fn(*a, **kw)
    except Exception as e:
        return False, str(e)


def main():
    ns = extract()
    cases = 0
    mism = 0

    bounded = {
        "ok": (1, 5, "bad", 0, False, 0), "zero": (1, 0, "bad", 0, False, 0),
        "negative": (1, -1, "bad", 0, False, 0),
        "high-ok": (1, 10, "bad", 0, True, 10), "high-over": (1, 11, "bad", 0, True, 10),
        "bool": (3, 1, "bad", 0, False, 0), "not-int": (0, 0, "bad", 0, False, 0),
        "positive": (1, 1, "must be positive", 1, False, 0),
        "msg": (1, 99, "limit must be between 1 and 10", 1, True, 10),
    }
    canonicals = {
        "small": ('{"b":1,"a":2}', 1024),
        "too-large": ('{"a":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}', 8),
        "exact": ('{"a":1}', 7),
        "null": (None, 1024),
    }
    scopes = {
        "complete": {"room_id": "R1", "home_install_id": "H1",
                     "authority_gateway_id": "G1", "authority_epoch": "7",
                     "member_id": "M1", "target_install_id": "T1",
                     "target_profile": "P1"},
        "incomplete": {"room_id": "R1", "home_install_id": "",
                       "authority_gateway_id": "G1", "authority_epoch": "7",
                       "member_id": "M1", "target_install_id": "T1",
                       "target_profile": "P1"},
    }
    superseded = {
        "newer": (8, "G1", "G1", 7), "same-same": (7, "G1", "G1", 7),
        "same-other": (7, "G2", "G1", 7), "older": (6, "G1", "G1", 7),
        "same-null": (7, None, "G1", 7),
    }
    identities = {
        "two": ({"room_id": "R1", "home_install_id": "H1",
                 "authority_gateway_id": "G1", "authority_epoch": "7",
                 "member_id": "M1", "target_install_id": "T1",
                 "target_profile": "P1", "task_id": "T9",
                 "execution_generation": "2"}, None),
        # missing execution_generation -> record[column] raises KeyError
        "with-missing": ({"room_id": "R1", "home_install_id": "H1",
                          "authority_gateway_id": "G1", "authority_epoch": "7",
                          "member_id": "M1", "target_install_id": "T1",
                          "target_profile": "P1", "task_id": "T9"}, None),
    }
    members = {
        "two": ['{"id":"a"}', '{"id":"b"}'],
        "one-not-object": ['{"id":"a"}', "[1,2]"],
        "empty": [],
    }
    user_events = {"simple": "evt-123", "padded": "  evt-123  ", "bad": "has space"}

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        fn = c.get("fn")
        label = c.get("case")
        cases += 1

        try:
            if fn == "bounded_int":
                kind, val, msg, low, hh, high = bounded[label]
                pyval = bool(val) if kind == 3 else (val if kind == 1 else None)
                want = call(ns["bounded_int"], pyval, error=HostedRoomError,
                            message=msg, low=low, **({"high": high} if hh else {}))
                if want[0]:
                    if c.get("rc") != 0 or c.get("out") != want[1]:
                        mism += 1
                        print(f"MISMATCH bounded_int case={label} C=(rc={c.get('rc')},out={c.get('out')}) PY={want!r}")
                else:
                    if c.get("rc") != -1 or c.get("err") != want[1]:
                        mism += 1
                        print(f"MISMATCH bounded_int case={label} C=(rc={c.get('rc')},err={c.get('err')!r}) PY=({want[1]!r})")

            elif fn == "canonical_json":
                jt, mb = canonicals[label]
                want = call(ns["canonical_json"], json.loads(jt) if jt else None,
                            error=HostedRoomError, label="payload", max_bytes=mb,
                            ensure_ascii=True)
                if want[0]:
                    if c.get("rc") != 0 or c.get("out") != want[1]:
                        mism += 1
                        print(f"MISMATCH canonical_json case={label} C={c.get('out')!r} PY={want[1]!r}")
                else:
                    if c.get("rc") != -1 or c.get("err") != want[1]:
                        mism += 1
                        print(f"MISMATCH canonical_json case={label} C={c.get('err')!r} PY={want[1]!r}")

            elif fn == "_room_grant_scope_key":
                claims = scopes[label]
                want = call(ns["_room_grant_scope_key"], claims)
                if want[0]:
                    if c.get("out") != want[1]:
                        mism += 1
                        print(f"MISMATCH _room_grant_scope_key case={label} C={c.get('out')!r} PY={want[1]!r}")
                else:
                    if c.get("err") != want[1]:
                        mism += 1
                        print(f"MISMATCH _room_grant_scope_key case={label} C={c.get('err')!r} PY={want[1]!r}")

            elif fn == "_reservation_superseded":
                re_, rg, gw, ep = superseded[label]
                row = {"authority_epoch": re_, "authority_gateway_id": rg}
                want = bool(ns["_reservation_superseded"](row, gw, ep))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _reservation_superseded case={label} C={got!r} PY={want!r}")

            elif fn == "_remote_run_identity":
                rec, cols = identities[label]
                want = call(ns["_remote_run_identity"], rec)
                if want[0]:
                    want = (True, list(want[1]))   # the Python returns a tuple
                    if c.get("out") != want[1]:
                        mism += 1
                        print(f"MISMATCH _remote_run_identity case={label} C={c.get('out')!r} PY={want[1]!r}")
                else:
                    # KeyError's str() is the quoted key; the C names the bare column
                    py_msg = want[1].strip("'\"")
                    if c.get("err") != py_msg:
                        mism += 1
                        print(f"MISMATCH _remote_run_identity case={label} C={c.get('err')!r} PY={py_msg!r}")

            elif fn == "_validate_members":
                mjs = members[label]
                want = call(ns["_validate_members"], [json.loads(m) for m in mjs])
                if want[0]:
                    if c.get("rc") != 0 or c.get("out") != want[1][1]:
                        mism += 1
                        print(f"MISMATCH _validate_members case={label} C={c.get('out')!r} PY={want[1][1]!r}")
                else:
                    if c.get("rc") != -1 or c.get("err") != want[1]:
                        mism += 1
                        print(f"MISMATCH _validate_members case={label} C={c.get('err')!r} PY={want[1]!r}")

            elif fn == "user_event_id":
                want = call(ns["user_event_id"], user_events[label])
                if want[0]:
                    if c.get("out") != want[1]:
                        mism += 1
                        print(f"MISMATCH user_event_id case={label} C={c.get('out')!r} PY={want[1]!r}")
                else:
                    if c.get("err") != want[1]:
                        mism += 1
                        print(f"MISMATCH user_event_id case={label} C={c.get('err')!r} PY={want[1]!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE hosted_rooms: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
