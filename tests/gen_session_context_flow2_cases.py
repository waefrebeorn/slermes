#!/usr/bin/env python3
"""Generate tests/fixtures/session_context_flow2.jsonl for the
gateway/session_context.py FLOW remainder oracle. >=5 cases per ported
function (7 funcs).

Each case is {"func", "in": {"args", "state", "io", ...}}. io scripts the
universal seam (a plain value reused, a list consumed per call, or
{"__script__": true, "ret"/"raise"}). The oracle re-drives the LIVE reference
from "in"; the C harness drives the port from the same "in".

Domain notes:
- ASCII fixture corpus. ContextVar state: "vars" (22 legacy names; missing
  key == _UNSET; {"__unset__": true} marker == explicit _UNSET), "turn_event",
  "history_delivery", "async_delivery", "engaged". Only present keys echo.
- Event/turn/source doubles are plain dicts (oracle materializes them to
  SimpleNamespace recursively). Anchor/actor attribute values: str/int/bool/
  null only (str(float)/str(dict) repr out of domain). metadata: dict/null
  (truthy non-mapping raises TypeError when anchors exist).
- Scripted raise kinds are plain-message classes (KeyError excluded:
  str(KeyError(k)) == repr(k)); bare raise dicts keep the uniform
  {"__raise__","message"} shape.
- A dict running_follow_up result is a CM carrier; null/str/int results pass
  through to the live `with` protocol (py3.14 TypeError).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "session_context_flow2.jsonl")

CASES = []


def add(func, args, io=None, state=None, no_io=False, **extra):
    inp = {"args": args, "state": state or {}}
    if not no_io:
        inp["io"] = io or {}
    inp.update(extra)
    CASES.append({"func": func, "in": inp})


def rs(type_, message=""):
    return {"__script__": True, "raise": {"type": type_, "message": message}}


def bare(type_, message=""):
    return {"__raise__": type_, "message": message}


AID = "HERMES_SESSION_ACTOR_ID"
ANAME = "HERMES_SESSION_ACTOR_NAME"
SKEY = "HERMES_SESSION_KEY"
PLAT = "HERMES_SESSION_PLATFORM"

# ── _runtime_cwd (15) ──
RC = "runtime_cwd.set_session_cwd"
add("_runtime_cwd", {"func": "set_session_cwd", "args": []}, {RC: None})
add("_runtime_cwd", {"func": "set_session_cwd", "args": ["/a", "b"]},
    {RC: {"ok": 1}})
add("_runtime_cwd", {"func": "clear_session_cwd", "args": []},
    {"runtime_cwd.clear_session_cwd": None})
add("_runtime_cwd", {"func": "set_session_cwd", "args": ["/x"]},
    {RC: rs("ValueError", "bad cwd")})
add("_runtime_cwd", {"func": "set_session_cwd", "args": []},
    {RC: rs("RuntimeError", "boom")})
add("_runtime_cwd", {"func": "set_session_cwd", "args": []},
    {RC: rs("SystemExit", "3")})
add("_runtime_cwd", {"func": "set_session_cwd", "args": []},
    {RC: rs("KeyboardInterrupt", "")})
add("_runtime_cwd", {"func": "set_session_cwd", "args": []},
    {RC: rs("GeneratorExit", "")})
add("_runtime_cwd", {"func": "set_session_cwd", "args": []},
    {RC: rs("CancelledError", "cancelled")})
add("_runtime_cwd", {"func": "set_session_cwd", "args": []},
    {RC: None}, import_fail=True)
add("_runtime_cwd", {"func": "set_session_cwd", "args": []},
    {RC: None}, runtime_cwd_attrs=["other_fn"])
add("_runtime_cwd", {"func": "set_session_cwd", "args": []}, {})
add("_runtime_cwd", {"func": "set_session_cwd", "args": []}, {RC: []})
add("_runtime_cwd", {"func": "set_session_cwd", "args": []},
    {RC: rs("UnicodeDecodeError", "codec")})
add("_runtime_cwd", {"func": "set_session_cwd", "args": []},
    {RC: bare("ValueError", "bare")})

# ── scoped_turn_actor (23) ──
TA_STATE = {"vars": {AID: "old_id", ANAME: "old_name"},
            "turn_event": {"user_id": "outer"}}
CM = {"cm": 1}
TA_IO = {"turn.running_follow_up": CM,
         "turn.running_follow_up.__enter__": None,
         "body": None,
         "turn.running_follow_up.__exit__": False}


def ta_io(**over):
    io = dict(TA_IO)
    io.update(over)
    return io


add("scoped_turn_actor", {"event": {"user_id": "u1", "user_name": "N1"}, "turn": {}},
    ta_io(), TA_STATE)
add("scoped_turn_actor",
    {"event": {"user_name": "N2", "source": {"user_id": "s1", "user_name": "S1"}},
     "turn": {}}, ta_io(), TA_STATE)
add("scoped_turn_actor", {"event": {"user_id": "", "user_name": "N"}, "turn": {}},
    ta_io(), TA_STATE)
add("scoped_turn_actor",
    {"event": {"user_id": None, "source": {"user_id": "s"}}, "turn": {}},
    ta_io(), TA_STATE)
add("scoped_turn_actor", {"event": None, "turn": {}}, ta_io(), TA_STATE)
add("scoped_turn_actor", {"event": "plain", "turn": {}}, ta_io(), TA_STATE)
add("scoped_turn_actor", {"event": {"user_id": 0, "user_name": False}, "turn": {}},
    ta_io(), TA_STATE)
add("scoped_turn_actor", {"event": {"user_id": 7, "user_name": True}, "turn": {}},
    ta_io(), TA_STATE)
add("scoped_turn_actor", {"event": {"user_id": "u"}, "turn": {}},
    ta_io(body=rs("ValueError", "body bad")), TA_STATE)
add("scoped_turn_actor", {"event": {"user_id": "u"}, "turn": {}},
    ta_io(body=rs("ValueError", "body bad"),
          **{"turn.running_follow_up.__exit__": True}), TA_STATE)
add("scoped_turn_actor", {"event": {"user_id": "u"}, "turn": {}},
    ta_io(body=rs("SystemExit", "1")), TA_STATE)
add("scoped_turn_actor", {"event": {"user_id": "u"}, "turn": {}},
    ta_io(**{"turn.running_follow_up.__exit__": rs("RuntimeError", "exit bad")}),
    TA_STATE)
add("scoped_turn_actor", {"event": {"user_id": "u"}, "turn": {}},
    ta_io(body=rs("ValueError", "body bad"),
          **{"turn.running_follow_up.__exit__": rs("RuntimeError", "exit bad")}),
    TA_STATE)
add("scoped_turn_actor", {"event": {"user_id": "u"}, "turn": {}},
    ta_io(**{"turn.running_follow_up": rs("ValueError", "rf bad")}), TA_STATE)
add("scoped_turn_actor", {"event": {"user_id": "u"}, "turn": None},
    ta_io(), TA_STATE)
add("scoped_turn_actor", {"event": {"user_id": "u"}, "turn": "x"},
    ta_io(), TA_STATE)
add("scoped_turn_actor", {"event": {"user_id": "u"}, "turn": {}},
    ta_io(**{"turn.running_follow_up.__enter__": rs("ValueError", "enter bad")}),
    TA_STATE)
add("scoped_turn_actor", {"event": {"user_id": "u"}, "turn": {}},
    ta_io(**{"turn.running_follow_up": None}), TA_STATE)
add("scoped_turn_actor", {"event": {"user_id": "u"}, "turn": {}},
    ta_io(**{"turn.running_follow_up": "x"}), TA_STATE)
add("scoped_turn_actor", {"event": {"user_id": "u"}, "turn": {}},
    {"turn.running_follow_up": CM,
     "turn.running_follow_up.__enter__": None,
     "turn.running_follow_up.__exit__": False}, TA_STATE)
add("scoped_turn_actor", {"event": {"user_id": "u"}, "turn": {}},
    no_io=True, state=TA_STATE)
add("scoped_turn_actor", {"event": {"user_id": "u"}, "turn": {}}, ta_io(), {})
add("scoped_turn_actor", {"event": {"user_id": "u"}, "turn": {}},
    ta_io(body=[]), TA_STATE)

# ── scoped_session_key (10) ──
add("scoped_session_key", {"session_key": "new"}, {"body": None},
    {"vars": {SKEY: "old"}})
add("scoped_session_key", {"session_key": "new"}, {"body": None}, {})
add("scoped_session_key", {"session_key": "new"}, {"body": None},
    {"vars": {SKEY: ""}})
add("scoped_session_key", {"session_key": "new"}, {"body": None},
    {"vars": {SKEY: {"__unset__": True}}})
add("scoped_session_key", {"session_key": "new"}, {"body": rs("ValueError", "b")},
    {"vars": {SKEY: "old"}})
add("scoped_session_key", {"session_key": "new"}, {"body": rs("SystemExit", "2")},
    {"vars": {SKEY: "old"}})
add("scoped_session_key", {"session_key": "new"}, {}, {"vars": {SKEY: "old"}})
add("scoped_session_key", {"session_key": "new"}, no_io=True,
    state={"vars": {SKEY: "old"}})
add("scoped_session_key", {"session_key": "new"}, {"body": []},
    {"vars": {SKEY: "old"}})
add("scoped_session_key", {"session_key": "new"}, {"body": [None, "x"]},
    {"vars": {SKEY: "old"}})

# ── bound_session_key (7) ──
add("bound_session_key", {}, {}, {})
add("bound_session_key", {}, {}, {"vars": {SKEY: {"__unset__": True}}})
add("bound_session_key", {}, {}, {"vars": {SKEY: ""}})
add("bound_session_key", {}, {}, {"vars": {SKEY: "k1"}})
add("bound_session_key", {}, {}, {"vars": {SKEY: "agent:main:telegram:c1"}})
add("bound_session_key", {}, {}, {"vars": {PLAT: "telegram"}})
add("bound_session_key", {}, {}, {"vars": {SKEY: None}})

# ── source_route_metadata (16) ──
add("source_route_metadata", {"source": None, "metadata": {"a": "1"}})
add("source_route_metadata", {"source": None, "metadata": None})
add("source_route_metadata", {"source": {"scope_id": "s1"}, "metadata": None})
add("source_route_metadata", {"source": {"parent_chat_id": "p1"},
                              "metadata": {"a": "1"}})
add("source_route_metadata", {"source": {"scope_id": "s1", "parent_chat_id": "p1"},
                              "metadata": {"scope_id": "old", "x": "2"}})
add("source_route_metadata", {"source": {"scope_id": "", "parent_chat_id": 0},
                              "metadata": {"a": "1"}})
add("source_route_metadata", {"source": {"scope_id": None, "parent_chat_id": False},
                              "metadata": {"a": "1"}})
add("source_route_metadata", {"source": {"scope_id": 42}, "metadata": None})
add("source_route_metadata", {"source": {"parent_chat_id": True}, "metadata": None})
add("source_route_metadata", {"source": {"scope_id": "s"}, "metadata": {}})
add("source_route_metadata", {"source": {"scope_id": "s"}, "metadata": "x"})
add("source_route_metadata", {"source": None, "metadata": "x"})
add("source_route_metadata", {"source": {"scope_id": "s"}, "metadata": [1]})
add("source_route_metadata", {"source": "str", "metadata": {"a": "1"}})
add("source_route_metadata", {"source": {"scope_id": "s"}, "metadata": 0})
add("source_route_metadata", {"source": {"scope_id": "s", "parent_chat_id": "p"},
                              "metadata": {"parent_chat_id": "old", "scope_id": "old2",
                                           "keep": "v"}})

# ── _bound_session_value (15) ──
BV_STATE = {"vars": {AID: "a", ANAME: "n", PLAT: "telegram", SKEY: "",
                     "HERMES_CRON_SESSION": "1",
                     "HERMES_CRON_AUTO_DELIVER_CHAT_ID": "c9",
                     "HERMES_BOGUS": "stray"},
            "turn_event": {"user_id": ""}}
add("_bound_session_value", {"name": AID}, {}, BV_STATE)
add("_bound_session_value", {"name": ANAME}, {}, BV_STATE)
add("_bound_session_value", {"name": AID}, {},
    {"vars": {AID: "a"}, "turn_event": {"user_id": "u"}})
add("_bound_session_value", {"name": AID}, {}, {"vars": {AID: "a"}, "turn_event": None})
add("_bound_session_value", {"name": AID}, {}, {"vars": {AID: "a"}})
add("_bound_session_value", {"name": PLAT}, {}, BV_STATE)
add("_bound_session_value", {"name": "HERMES_BOGUS"}, {}, BV_STATE)
add("_bound_session_value", {"name": "HERMES_SESSION_CHAT_ID"}, {}, BV_STATE)
add("_bound_session_value", {"name": SKEY}, {}, BV_STATE)
add("_bound_session_value", {"name": "HERMES_CRON_SESSION"}, {}, BV_STATE)
add("_bound_session_value", {"name": "HERMES_CRON_AUTO_DELIVER_CHAT_ID"}, {}, BV_STATE)
add("_bound_session_value", {"name": AID}, {},
    {"vars": {AID: "a"}, "turn_event": {"user_id": None}})
add("_bound_session_value", {"name": AID}, {}, {"vars": {AID: "a"}, "turn_event": "str"})
add("_bound_session_value", {"name": AID}, {}, {"vars": {AID: "a"}, "turn_event": {}})
add("_bound_session_value", {"name": AID}, {}, {"turn_event": {"user_id": ""}})

# ── session_history_delivery_supported (8) ──
add("session_history_delivery_supported", {}, {}, {"history_delivery": "1"})
add("session_history_delivery_supported", {}, {}, {"history_delivery": ""})
add("session_history_delivery_supported", {}, {}, {})
add("session_history_delivery_supported", {}, {},
    {"history_delivery": {"__unset__": True}})
add("session_history_delivery_supported", {}, {}, {"history_delivery": "0"})
add("session_history_delivery_supported", {}, {}, {"history_delivery": None})
add("session_history_delivery_supported", {}, {}, {"history_delivery": 1})
add("session_history_delivery_supported", {}, {}, {"history_delivery": "1 "})


def main():
    counts = {}
    for case in CASES:
        counts[case["func"]] = counts.get(case["func"], 0) + 1
    for func in counts:
        assert counts[func] >= 5, f"{func}: only {counts[func]} cases"
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            line = json.dumps(case, ensure_ascii=True)
            assert line.isascii(), "non-ASCII fixture"
            f.write(line + "\n")
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for func, n in sorted(counts.items()):
        print(f"  {func}: {n}")


if __name__ == "__main__":
    main()
