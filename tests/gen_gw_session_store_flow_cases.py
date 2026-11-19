#!/usr/bin/env python3
"""Generate tests/fixtures/gw_session_store_flow.jsonl — scripted-seam cases
for the gateway_session_store FLOW ports. Data only: no quarry code runs here.
Regenerate after changing the case shape: python tests/gen_gw_session_store_flow_cases.py
"""
import json
import os

OUT = os.path.join(os.path.dirname(__file__), "fixtures", "gw_session_store_flow.jsonl")
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def snap(sid="s1", **kw):
    d = {"stored_session_id": sid, "revision": 1, "execution_generation": 1, "info": {}}
    d.update(kw)
    return d


def st(snapshots=None, current=None, retry=None, states=None, final=False,
       launch_api_key=None, launch_api_key_typed=False, reader=None, handles=None,
       persistence=None, create_parameters=None):
    return {"snapshots": snapshots or {}, "current": current, "retry": retry,
            "connection": {"final": final, "launch_api_key": launch_api_key,
                           "launch_api_key_typed": launch_api_key_typed,
                           "states": states or {}},
            "reader": reader, "handles": handles, "persistence": persistence,
            "create_parameters": create_parameters}


def raise_(cls, msg):
    return {"__raise__": cls, "message": msg}


GCE = "GatewayClientError"

# ── session_label ──
CANON = "local-" + "abcd1234" + "0" * 56
case("session_label", session_id=CANON)
case("session_label", session_id="local-" + "ABCD1234" + "0" * 56)
case("session_label", session_id="local-abcd1234")
case("session_label", session_id=None)
case("session_label", session_id="")
case("session_label", session_id=5)
case("session_label", session_id="local-abcd123g" + "0" * 56)
case("session_label", session_id="localx-" + "abcd1234" + "0" * 56)

# ── _classic_only_policy ──
case("_classic_only_policy", launch={})
case("_classic_only_policy", launch={"run_budget": "2.5"})
case("_classic_only_policy", launch={"run_budget": 3})
case("_classic_only_policy", launch={"run_budget": 0})
case("_classic_only_policy", launch={"verbose": 1})
case("_classic_only_policy", launch={"verbose": ""})
case("_classic_only_policy", launch={"run_budget": "abc"})
case("_classic_only_policy", launch={"run_budget": []})
case("_classic_only_policy", launch=None)
case("_classic_only_policy", launch={"run_budget": " 1_0.5 "})
case("_classic_only_policy", launch={"run_budget": True})
case("_classic_only_policy", launch={"run_budget": None, "verbose": "x"})

# ── _transcript_view ──
for a, b in [(True, True), (True, False), (False, True), (False, False),
             (1, 0), ("", []), (None, None)]:
    case("_transcript_view", include_inactive=a, include_compacted=b)

# ── _window ──
case("_window", limit=-1, offset=0)
case("_window", limit=20, offset=5)
case("_window", limit=None, offset=0)
case("_window", limit=0, offset=0)
case("_window", limit=-5, offset=3)
case("_window", limit=5, offset=-1)
case("_window", limit="x", offset=0)
case("_window", limit=5, offset=None)

# ── set_session_yolo ──
for en in [True, False, 1, 0, "", "x", None]:
    case("set_session_yolo", session_id="s1", enabled=en,
         io={"connection.call": None})

# ── close_reader ──
case("close_reader", st=st())
case("close_reader", st=st(handles="h1", reader="r1", persistence="p1"),
     io={"handles.close": None})
case("close_reader", st=st(handles="h1", reader="r1", persistence="p1"),
     io={"handles.close": raise_(GCE, "close fail")})
case("close_reader", st=st(handles="h1", reader="r1"),
     io={"handles.close": raise_("SystemExit", "bye")})
case("close_reader", st=st(handles="h1"), io={"handles.close": None})

# ── __getattr__ ──
case("__getattr__", name="__bool__")
case("__getattr__", name="create_session", probe={"args": [1, 2]})
case("__getattr__", name="prune_empty_ghost_sessions")
case("__getattr__", name="get_session", probe={"args": ["s1"]}, io={"_db": None})
case("__getattr__", name="get_session", probe={"args": ["s1"]},
     io={"_db": "reader1", "reader.get_session": snap()})
case("__getattr__", name="sanitize_title", probe={"args": ["  Hi  "]},
     io={"sanitize_title": "Hi"})
case("__getattr__", name="session_yolo_enabled",
     probe={"args": [{"model_config": {"yolo": "1"}}]},
     io={"session_yolo_enabled": True})
case("__getattr__", name="session_yolo_enabled", probe={"args": [None]},
     io={"session_yolo_enabled": False})
case("__getattr__", name="session_yolo_enabled", probe={"args": ["x"]})
case("__getattr__", name="delete")
case("__getattr__", name="get_session", probe={"args": ["s1"]},
     io={"_db": "r1", "reader.get_session": raise_("KeyError", "s1")})

# ── _call ──
case("_call", method="session.resume", abandon=None, params={"session_id": "s1"},
     io={"connection.call": snap()})
case("_call", method="session.resume", abandon="poll", params={"session_id": "s1"},
     io={"connection.call": snap()})
case("_call", method="session.resume", abandon=0, params={"session_id": "s1"},
     io={"connection.call": snap()})
case("_call", method="session.resume", abandon=None, params={"method": "x"})
case("_call", method="session.resume", abandon="poll", params={"abandon": "y"})
case("_call", method="session.resume", abandon=None, params=None)
case("_call", method="session.resume", abandon=None, params={"session_id": "s1"},
     io={"connection.call": raise_(GCE, "disc")})
case("_call", method="session.resume", abandon=None,
     params={"session_id": "s1", "timeout": 5}, io={"connection.call": snap()})

# ── _adopt ──
case("_adopt", snapshot=snap(revision=2, execution_generation=3,
                             info={"title": "T"}, messages=[{"role": "user"}],
                             subscription_id="sub1"))
case("_adopt", snapshot={"revision": 1})
case("_adopt", snapshot=None)
case("_adopt", snapshot=snap(pending=[1, 2]))
case("_adopt", snapshot=snap(todo_state=None),
     st=st(states={"s1": {"todo_state": {"todos": []}}}))
case("_adopt", snapshot={"stored_session_id": "s1", "info": {}},
     st=st(states={"s1": {"revision": 9}}))
case("_adopt", snapshot={"stored_session_id": "s1", "info": {}},
     st=st(states={"s1": {"subscription_id": "old"}}))

# ── attach ──
case("attach", session_id="s1", st=st(snapshots={"s1": snap()}))
case("attach", session_id="s1", io={"connection.resume": snap()})
case("attach", session_id="s1", typed=True, io={"connection.resume": snap()})
case("attach", session_id="s1", io={"connection.resume": raise_(GCE, "not_found")})
case("attach", session_id="s1", io={"connection.resume": {"revision": 1}})
case("attach", session_id="s1", io={"connection.resume": None})

# ── open_session ──
CP = {"cwd": "/h", "source": "cli", "row_cwd": "", "request_id": "r1"}
case("open_session", cli={}, launch=None,
     io={"_create_params": CP, "connection.call": snap()})
case("open_session", cli={"session_id": "s1", "_resumed": True},
     st=st(snapshots={"s1": snap()}), io={"_check_resume_launch": None})
case("open_session", cli={"session_id": "s1", "_resumed": True},
     io={"connection.resume": raise_(GCE, "not_found")})
case("open_session", cli={"session_id": "s1", "_resumed": True},
     io={"connection.resume": raise_(GCE, "boom")})
case("open_session", cli={"session_id": "s1", "_resumed": True},
     st=st(snapshots={"s1": snap()}),
     io={"_check_resume_launch": raise_(GCE, "policy")})
case("open_session", cli={"_resumed": False, "_typed_api_key": "k1"},
     launch={"api_key": "k1"},
     io={"_create_params": CP, "connection.call": snap()})
case("open_session", cli={"_typed_api_key": "k2"}, launch={"api_key": "k1"},
     io={"_create_params": CP, "connection.call": snap()})
case("open_session", cli={"_resumed": True})
case("open_session", cli={"session_id": "s1", "_resumed": True},
     io={"connection.resume": snap(), "_check_resume_launch": None})

# ── create ──
case("create", launch=None, title=None, record_cwd=True,
     io={"_create_params": CP,
         "connection.call": snap(info={"config_warning": "watch out"})})
case("create", io={"_create_params": CP, "connection.call": snap(info={})})
case("create", st=st(retry=[{}, None]),
     io={"_create_params": CP, "connection.call": snap()})
case("create", st=st(retry=[{"params": CP}, None]),
     io={"connection.call": snap()})
case("create", st=st(retry=[{}, "poll"]),
     io={"_create_params": CP, "connection.call": snap()})
case("create", st=st(retry=[{}, None]),
     io={"_create_params": CP, "connection.call": raise_(GCE, "disc")})
case("create", io={"_create_params": CP, "connection.call": None})
case("create", io={"_create_params": CP,
                   "connection.call": snap(info={"config_warning": 5})})
case("create", io={"_create_params": None})

# ── branch ──
RS = {"revision": 5, "execution_generation": 2}
RC = {"branched_session_id": "c1"}
SC = snap("c1")
case("branch", session_id="s1", title="child",
     io={"connection.call": [RS, RC], "uuid4": "hex1", "connection.resume": SC})
case("branch", session_id="s1", st=st(retry=[{"child": "c1"}, None],
                                      snapshots={"c1": SC}))
case("branch", session_id="s1",
     st=st(retry=[{"mutate": {"session_id": "s1", "request_id": "hex0",
                              "expected_revision": 5, "expected_generation": 2,
                              "operation": "branch", "payload": {}}}, None]),
     io={"connection.call": RC, "connection.resume": SC})
case("branch", session_id="s1",
     io={"connection.call": {"execution_generation": 2}, "uuid4": "hex1"})
case("branch", session_id="s1",
     io={"connection.call": [RS, {}], "uuid4": "hex1"})
case("branch", session_id="s1",
     io={"connection.call": [RS, None], "uuid4": "hex1"})
case("branch", session_id="s1",
     io={"connection.call": [RS, "x"], "uuid4": "hex1"})
case("branch", session_id="s1", title=None,
     io={"connection.call": [RS, RC], "uuid4": "hex1", "connection.resume": SC})
case("branch", session_id="s1",
     io={"connection.call": raise_(GCE, "disc")})

# ── _mutate ──
MS = {"revision": 5, "execution_generation": 2}
case("_mutate", session_id="s1", operation="delete", payload={}, snapshot=MS,
     io={"uuid4": "hex1", "connection.call": {"ok": True}})
case("_mutate", session_id="s1", operation="rename", payload={"title": "T"},
     snapshot={"revision": 5}, io={"uuid4": "hex1", "connection.call": {"ok": True}})
case("_mutate", session_id="s1", operation="delete", payload={}, snapshot=None,
     io={"connection.call": [MS, {"ok": True}], "uuid4": "hex1"})
case("_mutate", session_id="s1", operation="delete", payload={}, snapshot={},
     io={"connection.call": [MS, {"ok": True}], "uuid4": "hex1"})
case("_mutate", session_id="s1", operation="delete", payload={},
     snapshot={"revision": 5}, io={"uuid4": "hex1"})
case("_mutate", session_id="s1", operation="delete", payload={},
     snapshot={"execution_generation": 2}, io={"uuid4": "hex1"})
case("_mutate", session_id="s1", operation="delete", payload={}, snapshot=None,
     io={"connection.call": raise_(GCE, "disc")})
case("_mutate", session_id="s1", operation="compress", payload={"strategy": "x"},
     snapshot=MS, io={"uuid4": "hex1", "connection.call": {"ok": True}})

# ── set_session_title ──
TR = {"stored_session_id": "s1", "revision": 5}
case("set_session_title", session_id="s1", title="New",
     st=st(snapshots={"s1": snap()}),
     io={"connection.call": [TR, {"ok": True}], "uuid4": "hex1"})
case("set_session_title", session_id="s1", title="New",
     st=st(snapshots={"s1": snap()}),
     io={"connection.call": [{"stored_session_id": "owner", "revision": 5},
                             {"ok": True}],
         "uuid4": "hex1", "_row_revision": 9})
case("set_session_title", session_id="s1", title="New",
     io={"connection.call": [TR, {"ok": True}], "uuid4": "hex1"})
case("set_session_title", session_id="s1", title="New",
     io={"connection.call": [TR, raise_(GCE, "title_conflict")],
         "uuid4": "hex1", "_title_holder": "other"})
case("set_session_title", session_id="s1", title="New",
     io={"connection.call": [TR, raise_(GCE, "title_conflict")],
         "uuid4": "hex1", "_title_holder": None})
case("set_session_title", session_id="s1", title="New",
     io={"connection.call": [TR, raise_(GCE, "title_conflict")],
         "uuid4": "hex1", "_title_holder": ""})
case("set_session_title", session_id="s1", title="New",
     io={"connection.call": [TR, raise_(GCE, "title_conflict")],
         "uuid4": "hex1", "_title_holder": raise_("RuntimeError", "db down")})
case("set_session_title", session_id="s1", title="New",
     io={"connection.call": {"stored_session_id": "s1"}, "uuid4": "hex1"})
case("set_session_title", session_id="s1", title="New",
     io={"connection.call": {"revision": 5}})

# ── delete_session ──
DS = {"stored_session_id": "s1", "revision": 5, "execution_generation": 2}
case("delete_session", session_id="s1",
     st=st(snapshots={"s1": snap()}, states={"s1": {"subscription_id": "sub1"}}),
     io={"connection.call": [DS, {"ok": True}], "uuid4": "hex1"})
case("delete_session", session_id="s1",
     io={"connection.call": [DS, {"ok": True}], "uuid4": "hex1"})
case("delete_session", session_id="s1",
     io={"connection.call": [dict(DS, subscription_id="sub2"),
                             raise_(GCE, "refused"), {"ok": True}],
         "uuid4": "hex1"})
case("delete_session", session_id="s1",
     io={"connection.call": [dict(DS, subscription_id="sub2"),
                             raise_(GCE, "refused"), raise_(GCE, "detach_fail")],
         "uuid4": "hex1"})
case("delete_session", session_id="s1",
     st=st(states={"s1": {"subscription_id": "sub1"}}),
     io={"connection.call": [DS, raise_(GCE, "refused")], "uuid4": "hex1"})
case("delete_session", session_id="s1",
     io={"connection.call": [DS, raise_(GCE, "refused")], "uuid4": "hex1"})
case("delete_session", session_id="s1",
     st=st(states={"owner": {"subscription_id": "subO"}}),
     io={"connection.call": [dict(DS, session_id="owner", stored_session_id="owner"),
                             {"ok": True}], "uuid4": "hex1"})
case("delete_session", session_id="s1",
     io={"connection.call": [DS, raise_("SystemExit", "bye")], "uuid4": "hex1"})
case("delete_session", session_id="s1",
     io={"connection.call": [{}, DS, {"ok": True}], "uuid4": "hex1"})

# ── end_session ──
case("end_session", session_id="s1", reason=None,
     st=st(snapshots={"s1": snap()}, current={"ref": "s1"},
           states={"s1": {"subscription_id": "sub1"}}),
     io={"connection.start": None, "connection.call": {"ok": True}})
case("end_session", session_id="s1",
     st=st(snapshots={"s1": snap()}, current={"ref": "s1"}, final=True,
           states={"s1": {"subscription_id": "sub1"}}))
case("end_session", session_id="s1",
     st=st(snapshots={"s1": snap()}, current={"ref": "s1"}))
case("end_session", session_id="s1",
     st=st(snapshots={"s1": snap()}, current={"ref": "s1"},
           states={"s1": {"subscription_id": "sub1", "gaps": 1, "attached_gaps": 0}}),
     io={"connection.start": None, "connection.reattach": None,
         "connection.call": {"ok": True}})
case("end_session", session_id="s1",
     st=st(snapshots={"s1": snap()}, current={"ref": "s1"},
           states={"s1": {"subscription_id": "sub1"}}),
     io={"connection.start": raise_(GCE, "redial_fail")})
case("end_session", session_id="s1",
     st=st(snapshots={"s1": snap()}, current={"ref": "s1"},
           states={"s1": {"subscription_id": "sub1", "gaps": 1, "attached_gaps": 0}}),
     io={"connection.start": None, "connection.reattach": raise_(GCE, "reattach_fail")})
case("end_session", session_id="s1",
     st=st(snapshots={"s1": snap()}, current={"ref": "s1"},
           states={"s1": {"subscription_id": ""}}),
     io={"connection.start": None})
case("end_session", session_id="s1", reason="quit",
     st=st(snapshots={"s1": snap()}, current={"ref": "s1"},
           states={"s1": {"subscription_id": "sub1"}}),
     io={"connection.start": None, "connection.call": {"ok": True}})
case("end_session", session_id="s1", reason="",
     st=st(snapshots={"s1": snap()}, current={"ref": "s1"},
           states={"s1": {"subscription_id": "sub1"}}),
     io={"connection.start": None, "connection.call": raise_(GCE, "detach_fail")})
case("end_session", session_id="s1",
     st=st(snapshots={"s1": snap()}, current=snap(),
           states={"s1": {"subscription_id": "sub1"}}),
     io={"connection.start": None, "connection.call": {"ok": True}})
case("end_session", session_id="s1", st=st())

# ── delete_session_if_empty ──
IEC = "persistence.sessions.is_empty_candidate"
PS = st(persistence="p1")
case("delete_session_if_empty", session_id="s1", io={"_db": None})
case("delete_session_if_empty", session_id="s1", st=PS,
     io={"_db": "r1", IEC: raise_("ValueError", "probe_fail")})
case("delete_session_if_empty", session_id="s1", st=PS,
     io={"_db": "r1", IEC: raise_("SystemExit", "bye")})
case("delete_session_if_empty", session_id="s1", st=PS, io={"_db": "r1", IEC: False})
case("delete_session_if_empty", session_id="s1", st=PS,
     io={"_db": "r1", IEC: True, "connection.call": [DS, {"ok": True}],
         "uuid4": "hex1"})
case("delete_session_if_empty", session_id="s1", st=PS,
     io={"_db": "r1", IEC: True,
         "connection.call": [DS, raise_(GCE, "refused")], "uuid4": "hex1"})
case("delete_session_if_empty", session_id="s1", st=PS, io={"_db": "r1", IEC: 0})
case("delete_session_if_empty", session_id="s1", st=PS,
     io={"_db": "", IEC: True, "connection.call": [DS, {"ok": True}],
         "uuid4": "hex1"})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for k in sorted(counts):
        print("  %-28s %d" % (k, counts[k]))
    short = [k for k, v in counts.items() if v < 5]
    if short:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % short)


if __name__ == "__main__":
    main()