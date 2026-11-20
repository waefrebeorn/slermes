#!/usr/bin/env python3
"""Generate tests/fixtures/session_controls_flow2.jsonl for the
session-controls FLOW remainder oracle. >=5 cases per ported function (17).

Each case is {"func", "in": {"args", "state", "io"}}. io scripts the universal
seam (a plain value reused, a list consumed per call, or {"__script__": true,
"ret"/"raise"}). Claimed-sibling rets (self.submission / authority receipts)
are computed to match the live dataclass asdict shapes. The oracle re-drives
the LIVE reference from "in"; the C harness drives the port from the same "in".
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "session_controls_flow2.jsonl")
CAPS = sorted(["session:read", "session:submit", "session:control",
               "session:approve", "session:respond", "session:create"])


def actor(subject="u1", caps=None, tid="tx1", pid="p1"):
    return {"subject": subject, "profile_id": pid,
            "capabilities": sorted(caps if caps is not None else CAPS), "transport_id": tid}


def ref(sid="s1", pid="p1"):
    return {"profile_id": pid, "session_id": sid}


def base_state():
    return {
        "authority": "a1", "transport": "t1",
        "actor": actor(), "identity": {"user_id": "u1", "profile_scope": "host"},
        "operator": False, "native_owner": False,
        "subscriptions": {"s1": "sub1"}, "replay_cursors": {},
        "reconnects": [], "handed_back": [], "siblings": {},
        "authorities": {"a1": {"profile_id": "p1", "instance_id": "i1", "epoch": 7}},
        "sessions": {"s1": {"subscribers": ["sub1"]}, "s2": {"subscribers": ["sub2"]}},
        "events": {"tx1": "t1"},
    }


def sc(ret):
    return {"__script__": True, "ret": ret}


def raise_spec(type_, message="", **kw):
    d = {"__script__": True, "raise": {"type": type_, "message": message}}
    d["raise"].update(kw)
    return d


def refusal(reason, **kw):
    return raise_spec("AuthorityRefusal", reason, reason=reason, **kw)


def R(aid="ad1", status="terminal", outcome="finished", seq=1, gen=3, epoch=7, sid="s1"):
    return {"admission_id": aid, "ref": ref(sid), "sequence": seq, "status": status,
            "outcome": outcome, "authority_epoch": epoch, "execution_generation": gen}


def SUB(rid="r1", sid="s1", text="hi"):
    return {"request_id": rid, "ref": ref(sid), "payload": {"text": text}, "intent": "queue"}


def row(sid="s1", title="T", source="cli", started=100, mc=2, model="m1"):
    return {"session_id": sid, "title": title, "source": source,
            "started_at": started, "message_count": mc, "model": model}


def entry(sid="s1", r=None, closed=False, state="running"):
    return {"session_id": sid, "row": r if r is not None else row(sid),
            "closed": closed, "handle": {"execution_state": state}}


CASES = []


def add(func, args, io=None, state=None, **state_over):
    st = base_state() if state is None else state
    st.update(state_over)
    CASES.append({"func": func, "in": {"args": args, "state": st, "io": dict(io or {})}})


# ── __init__ ──
INIT_IO = {"principal_subject": "u1", "uuid4_hex": "hex1", "bind_native_transport": None}
add("__init__", {"authority": "a1", "transport": "t1",
                 "identity": {"user_id": "u1"}, "operator": False}, dict(INIT_IO))
add("__init__", {"authority": "a1", "transport": "t1",
                 "identity": {"user_id": "u1", "capabilities": ["session:read", "session:operator"]},
                 "operator": True}, dict(INIT_IO))
add("__init__", {"authority": "a1", "transport": "t1", "identity": {}, "operator": False},
    dict(INIT_IO))
add("__init__", {"authority": "a1", "transport": "t1",
                 "identity": {"user_id": "u1", "instance_id": "other"}, "operator": False},
    dict(INIT_IO))
add("__init__", {"authority": "a1", "transport": "t1",
                 "identity": {"user_id": "u1", "instance_id": "i1", "profile_id": "p9",
                              "native_bootstrap": True}, "operator": False},
    dict(INIT_IO, principal_subject="u9"))
add("__init__", {"authority": "a1", "transport": "t1",
                 "identity": {"user_id": "u1"}, "operator": False},
    dict(INIT_IO, principal_subject=raise_spec("RuntimeError", "boom")))
add("__init__", {"authority": "a1", "transport": "t1",
                 "identity": {"user_id": "u1", "capabilities": ["session:operator", "session:read"]},
                 "operator": True}, dict(INIT_IO))
add("__init__", {"authority": "a1", "transport": "t1",
                 "identity": {"user_id": "u1"}, "operator": False},
    dict(INIT_IO, uuid4_hex=raise_spec("SystemExit", "bye")))

# ── host_scoped ──
add("host_scoped", {}, {}, identity={"profile_scope": "host"})
add("host_scoped", {}, {}, identity={"profile_scope": "profile"})
add("host_scoped", {}, {}, identity={})
add("host_scoped", {}, {}, identity={"profile_scope": None})
add("host_scoped", {}, {}, identity={"profile_scope": 5})

# ── _voice_stop_key ──
add("_voice_stop_key", {"ref": ref(), "params": {"submission_id": "r1"}}, {})
add("_voice_stop_key", {"ref": ref(), "params": {"submission_id": "", "input_id": "i1"}}, {})
add("_voice_stop_key", {"ref": ref(), "params": {"submission_id": None, "input_id": "i2"}}, {})
add("_voice_stop_key", {"ref": ref(), "params": {}}, {})
add("_voice_stop_key", {"ref": ref(), "params": {"submission_id": 5}}, {})
add("_voice_stop_key", {"ref": ref(), "params": {"submission_id": "", "input_id": ""}}, {})

# ── _resume_transcript ──
HIST = [{"row_id": "r1", "text": "a"}, {"row_id": "r2", "text": "b"}]
add("_resume_transcript", {"ref": ref(), "snapshot": {"transcript_target": "s1"},
                           "history": HIST, "omit": False},
    {"_todo_state_from_history": {"items": []}})
add("_resume_transcript", {"ref": ref(), "snapshot": {"transcript_target": "s1"},
                           "history": HIST, "omit": True},
    {"authority.unless_deleted": sc([{"items": [{"id": 1}]}, 5])})
add("_resume_transcript", {"ref": ref(), "snapshot": {"transcript_target": "s1"},
                           "history": [], "omit": False},
    {"_todo_state_from_history": None})
add("_resume_transcript", {"ref": ref(), "snapshot": {"transcript_target": "s1"},
                           "history": HIST, "omit": True},
    {"authority.unless_deleted": refusal("not_found")})
add("_resume_transcript", {"ref": ref(), "snapshot": {"transcript_target": "s1"},
                           "history": HIST, "omit": 1},
    {"authority.unless_deleted": sc([None, 0])})
add("_resume_transcript", {"ref": ref(), "snapshot": {"transcript_target": "s1"},
                           "history": HIST, "omit": True},
    {"authority.unless_deleted": None})
add("_resume_transcript", {"ref": ref(), "snapshot": {"transcript_target": "s1"},
                           "history": HIST, "omit": False},
    {"_todo_state_from_history": raise_spec("RuntimeError", "x")})

# ── create ──
add("create", {"ref": ref(), "params": {}}, {"self._create": {"session_id": "s1"}})
add("create", {"ref": ref(), "params": {"title": "T"}},
    {"validate_title": "T", "self._create": {"session_id": "s1"}})
add("create", {"ref": ref(), "params": {"title": "T"}},
    {"validate_title": None, "self._create": {"session_id": "s1"}})
add("create", {"ref": ref(), "params": {"title": "T", "cwd": "/x"}},
    {"validate_title": "T", "self._create": {"session_id": "s1"}})
add("create", {"ref": ref(), "params": {"title": "T"}},
    {"validate_title": raise_spec("ValueError", "bad title")})
add("create", {"ref": ref(), "params": {"title": "T"}},
    {"validate_title": "T", "self._create": {"session_id": "s1"}},
    authorities={"a1": {"profile_id": "p1", "instance_id": "i1", "epoch": 7,
                        "_titled_create_lock": True}})
add("create", {"ref": ref(), "params": {"title": "T"}},
    {"validate_title": "T", "self._create": refusal("permission_denied")})

# ── describe ──
add("describe", {"ref": ref(), "params": {}},
    {"self.ping": {"pong": True}, "resolved_skin": {"theme": "dark"}})
add("describe", {"ref": ref(), "params": {}}, {"self.ping": refusal("permission_denied")})
add("describe", {"ref": ref(), "params": {}},
    {"self.ping": {"pong": True}, "resolved_skin": raise_spec("RuntimeError", "skin")})
add("describe", {"ref": ref(), "params": {}}, {"self.ping": None, "resolved_skin": {}})
add("describe", {"ref": ref(), "params": {"x": 1}},
    {"self.ping": {"pong": True}, "resolved_skin": {"theme": "light"}})

# ── info ──
add("info", {"ref": ref(), "params": {"session_id": "s1"}},
    {"authority.resolve": None, "snapshot_info": {"title": "T"}})
add("info", {"ref": ref(), "params": {}}, {})
add("info", {"ref": ref(), "params": {"session_id": "s1", "extra": 1}}, {})
add("info", {"ref": ref(), "params": {"session_id": "s1"}},
    {"authority.resolve": refusal("not_found")})
add("info", {"ref": ref(), "params": {"session_id": "s1"}},
    {"authority.resolve": None, "snapshot_info": raise_spec("RuntimeError", "x")})

# ── list_sessions ──
add("list_sessions", {"ref": ref(), "params": {}}, {},
    actor=actor(caps=["session:submit"]))
add("list_sessions", {"ref": ref(), "params": {}},
    {"check_page_params": None, "live_entries": sc([entry("s1")]),
     "live_picker_page": sc([{"session_id": "s1"}])})
add("list_sessions", {"ref": ref(), "params": {"limit": 0}}, {})
add("list_sessions", {"ref": ref(), "params": {"limit": True}}, {})
add("list_sessions", {"ref": ref(), "params": {"picker": 1}}, {})
add("list_sessions", {"ref": ref(), "params": {"workspace": ""}}, {})
add("list_sessions", {"ref": ref(), "params": {"scope": "bogus"}}, {})
add("list_sessions", {"ref": ref(), "params": {"title": "T", "scope": "live"}}, {})
add("list_sessions", {"ref": ref(), "params": {"scope": "stored", "limit": 5}},
    {"check_page_params": None, "stored_picker_sessions": sc([{"session_id": "s9"}])})
add("list_sessions", {"ref": ref(), "params": {"title": "T"}},
    {"check_page_params": None, "resolve_titled_session": None})
add("list_sessions", {"ref": ref(), "params": {"title": "T"}},
    {"check_page_params": None, "resolve_titled_session": ref("s1"),
     "store.sessions.get": [row("s1"), row("s1", title="Root")],
     "store.sessions.resolve_by_title": "s1"})
add("list_sessions", {"ref": ref(), "params": {"title": "T"}},
    {"check_page_params": None, "resolve_titled_session": ref("s1"),
     "store.sessions.get": [None, None], "store.sessions.resolve_by_title": None})
add("list_sessions", {"ref": ref(), "params": {"include_hidden": False}},
    {"check_page_params": None,
     "live_entries": sc([entry("s1", row("s1", source="kanban")),
                         entry("s2", row("s2", source="cli", started=50), closed=True),
                         entry("s3", row("s3", source="cli", started=200)),
                         entry("s4", dict(row("s4", source="cli", started=200), hidden=True)),
                         entry("s5", row("s5", source="cli", started=10))]),
     "live_picker_page": sc([{"session_id": "s3"}, {"session_id": "s5"}])})
add("list_sessions", {"ref": ref(), "params": {}},
    {"check_page_params": refusal("invalid_params")})
add("list_sessions", {"ref": ref(), "params": {"badkey": 1}}, {})
add("list_sessions", {"ref": ref(), "params": {}},
    {"check_page_params": None, "live_entries": raise_spec("RuntimeError", "x")})
add("list_sessions", {"ref": ref(), "params": {"title": "T"}},
    {"check_page_params": None, "resolve_titled_session": ref("s1"),
     "store.sessions.get": [{"session_id": "s1"}], "store.sessions.resolve_by_title": None})

# ── submit ──
add("submit", {"ref": ref(), "params": {"text": "stop"}},
    {"self._voice_stop_replay": True, "self._record_voice_stop": None})
add("submit", {"ref": ref(), "params": {"text": "stop"}},
    {"self._voice_stop_replay": False, "self._typed_voice_stop": True,
     "self._record_voice_stop": None})
add("submit", {"ref": ref(), "params": {"text": "hi", "submission_id": "r1"}},
    {"self._voice_stop_replay": False, "self._typed_voice_stop": False,
     "self.submission": SUB(), "authority.submit": R()})
add("submit", {"ref": ref(), "params": {}}, {"self._voice_stop_replay": refusal("permission_denied")})
add("submit", {"ref": ref(), "params": {}},
    {"self._voice_stop_replay": False, "self._typed_voice_stop": raise_spec("RuntimeError", "x")})
add("submit", {"ref": ref(), "params": {}},
    {"self._voice_stop_replay": False, "self._typed_voice_stop": False,
     "self.submission": refusal("invalid_params")})
add("submit", {"ref": ref(), "params": {}},
    {"self._voice_stop_replay": False, "self._typed_voice_stop": False,
     "self.submission": SUB(), "authority.submit": refusal("session_busy")})
add("submit", {"ref": ref(), "params": {}},
    {"self._voice_stop_replay": False, "self._typed_voice_stop": False,
     "self.submission": SUB(), "authority.submit": R()}, operator=True)

# ── mutate ──
add("mutate", {"ref": ref(), "params": {"title": "N"}},
    {"mutate_session": {"branched_session_id": "s2"}, "self.resume": {"session_id": "s2"}})
add("mutate", {"ref": ref(), "params": {}}, {"mutate_session": {"branched_session_id": "s1"}})
add("mutate", {"ref": ref(), "params": {}}, {"mutate_session": {"ok": True}})
add("mutate", {"ref": ref(), "params": {}},
    {"mutate_session": raise_spec("TurnLeaseLost", session_id="s1")})
add("mutate", {"ref": ref(), "params": {}},
    {"mutate_session": raise_spec("CompressionInProgress", session_id="s1")})
add("mutate", {"ref": ref(), "params": {}},
    {"mutate_session": refusal("revision_conflict"),
     "store.sessions.get": {"runtime_revision": 9, "runtime_generation": 3}})
add("mutate", {"ref": ref(), "params": {}},
    {"mutate_session": refusal("stale_generation"), "store.sessions.get": None})
add("mutate", {"ref": ref(), "params": {}}, {"mutate_session": refusal("permission_denied")})
add("mutate", {"ref": ref(), "params": {}}, {"mutate_session": raise_spec("SystemExit", "bye")})
add("mutate", {"ref": ref(), "params": {}},
    {"mutate_session": refusal("revision_conflict"),
     "store.sessions.get": raise_spec("RuntimeError", "store down")})
add("mutate", {"ref": ref(), "params": {}},
    {"mutate_session": {"branched_session_id": "s2"}, "self.resume": refusal("not_found")})
add("mutate", {"ref": ref(), "params": {}}, {"mutate_session": [1, 2]})
add("mutate", {"ref": ref(), "params": {}},
    {"mutate_session": refusal("stale_generation"),
     "store.sessions.get": {"runtime_revision": 4, "runtime_generation": 8}})

# ── receipt ──
add("receipt", {"ref": ref(), "params": {"admission_id": "ad1"}},
    {"authority.receipt": R()})
add("receipt", {"ref": ref(), "params": {"admission_id": "ad1", "include_result": True}},
    {"authority.receipt": R(), "admission_result": {"result": {"x": 1}, "usage": {"t": 2}}})
add("receipt", {"ref": ref(), "params": {"admission_id": "ad1", "include_result": True}},
    {"authority.receipt": R(), "admission_result": None})
add("receipt", {"ref": ref(), "params": {"admission_id": "ad1", "include_result": True}},
    {"authority.receipt": R(status="started")})
add("receipt", {"ref": ref(), "params": {"admission_id": "ad1", "include_result": True}},
    {"authority.receipt": R(), "admission_result": {"result": {}}})
add("receipt", {"ref": ref(), "params": {"admission_id": "ad1"}},
    {"authority.receipt": refusal("not_found")})
add("receipt", {"ref": ref(), "params": {"admission_id": "ad1", "include_result": True}},
    {"authority.receipt": R(), "admission_result": raise_spec("RuntimeError", "x")})
add("receipt", {"ref": ref(), "params": {"admission_id": "ad1", "include_result": 1}},
    {"authority.receipt": R()})

# ── resolve_unknown ──
RU_PARAMS = {"session_id": "s1", "admission_id": "ad1", "execution_generation": 3}
add("resolve_unknown", {"ref": ref(), "params": dict(RU_PARAMS)},
    {"authority.resolve_unknown": R()})
add("resolve_unknown", {"ref": ref(), "params": {"session_id": "s1", "admission_id": "ad1"}}, {})
add("resolve_unknown", {"ref": ref(), "params": dict(RU_PARAMS, extra=1)}, {})
add("resolve_unknown", {"ref": ref(), "params": dict(RU_PARAMS)},
    {"authority.resolve_unknown": refusal("unknown_execution")})
add("resolve_unknown", {"ref": ref(), "params": {}}, {})

# ── respond ──
RESP_PARAMS = {"session_id": "s1", "execution_generation": 3, "prompt_id": "p1", "choice": "yes"}
add("respond", {"ref": ref(), "params": dict(RESP_PARAMS)}, {"authority.respond": {"ok": True}})
add("respond", {"ref": ref(), "params": {"session_id": "s1", "execution_generation": 3,
                                         "prompt_id": "p1"}}, {})
add("respond", {"ref": ref(), "params": dict(RESP_PARAMS, extra=1)}, {})
add("respond", {"ref": ref(), "params": dict(RESP_PARAMS)},
    {"authority.respond": refusal("not_found")})
add("respond", {"ref": ref(), "params": dict(RESP_PARAMS, choice=None)},
    {"authority.respond": {"ok": True}})

# ── respond_clarify ──
CL_PARAMS = {"session_id": "s1", "execution_generation": 3, "prompt_id": "p1", "answer": "42"}
add("respond_clarify", {"ref": ref(), "params": dict(CL_PARAMS)}, {"authority.respond": {"ok": True}})
add("respond_clarify", {"ref": ref(), "params": {"session_id": "s1", "execution_generation": 3,
                                                "prompt_id": "p1"}}, {})
add("respond_clarify", {"ref": ref(), "params": dict(RESP_PARAMS)}, {})
add("respond_clarify", {"ref": ref(), "params": dict(CL_PARAMS)},
    {"authority.respond": refusal("not_found")})
add("respond_clarify", {"ref": ref(), "params": dict(CL_PARAMS, answer={"text": "x"})},
    {"authority.respond": {"ok": True}})

# ── _resolve_response ──
FRAME = {"id": "q1", "result": {"value": "ok"}}
add("_resolve_response", {"frame": FRAME},
    {"may_settle": False, "resolve": {"__script__": True, "ret": False,
                                     "asks": [{"session_id": "s1", "events": {"e": 1}}]}})
add("_resolve_response", {"frame": FRAME},
    {"may_settle": True, "resolve": {"__script__": True, "ret": True,
                                     "asks": [{"session_id": "s1", "events": {}}]}})
add("_resolve_response", {"frame": FRAME}, {"resolve": False})
add("_resolve_response", {"frame": FRAME},
    {"may_settle": [False, True], "resolve": {"__script__": True, "ret": False,
                                              "asks": [{"session_id": "s1", "events": {}}]}},
    siblings={"p2": {"authority": "a2", "actor": actor(tid="tx2", pid="p2")}},
    authorities={"a1": {"profile_id": "p1", "instance_id": "i1", "epoch": 7},
                 "a2": {"profile_id": "p2", "instance_id": "i2", "epoch": 7}})
add("_resolve_response", {"frame": FRAME},
    {"may_settle": raise_spec("RuntimeError", "x"),
     "resolve": {"__script__": True, "ret": False, "asks": [{"session_id": "s1", "events": {}}]}})
add("_resolve_response", {"frame": FRAME}, {"resolve": raise_spec("RuntimeError", "x")})
add("_resolve_response", {"frame": FRAME}, {})
add("_resolve_response", {"frame": FRAME},
    {"may_settle": [True, False], "resolve": {"__script__": True, "ret": True,
                                              "asks": [{"session_id": "s1", "events": {}},
                                                       {"session_id": "s2", "events": {}}]}})

# ── close ──
add("close", {}, {"forget": None, "authority.detach": None, "unbind_native_transport": None})
add("close", {}, {"sibling.close": None, "forget": None, "authority.detach": None,
                  "unbind_native_transport": None},
    siblings={"p2": {"authority": "a2", "actor": actor(tid="tx2", pid="p2")}},
    authorities={"a1": {"profile_id": "p1", "instance_id": "i1", "epoch": 7},
                 "a2": {"profile_id": "p2", "instance_id": "i2", "epoch": 7}})
add("close", {}, {"forget": None, "unbind_native_transport": None},
    sessions={"s1": {"subscribers": ["other"]}})
add("close", {}, {"forget": None, "unbind_native_transport": None}, sessions={})
add("close", {}, {"forget": None, "authority.detach": None,
                  "arm_detached_orphan_close": None, "unbind_native_transport": None},
    handed_back=["s2"])
add("close", {}, {"sibling.close": refusal("runtime_draining")},
    siblings={"p2": {"authority": "a2", "actor": actor(tid="tx2", pid="p2")}},
    authorities={"a1": {"profile_id": "p1", "instance_id": "i1", "epoch": 7},
                 "a2": {"profile_id": "p2", "instance_id": "i2", "epoch": 7}})
add("close", {}, {"forget": None, "authority.detach": refusal("not_found"),
                  "unbind_native_transport": None})
add("close", {}, {"forget": raise_spec("RuntimeError", "x")})
add("close", {}, {"forget": None, "authority.detach": None,
                  "unbind_native_transport": raise_spec("SystemExit", "bye")})

# ── _spawn_notice ──
add("_spawn_notice", {"ref": ref()}, {"create_task": "task1"})
add("_spawn_notice", {"ref": ref()}, {"create_task": "task2"}, notice_tasks=["task1"])
add("_spawn_notice", {"ref": ref()}, {"create_task": "task1"}, notice_tasks=["task1"])
add("_spawn_notice", {"ref": ref()}, {"create_task": raise_spec("RuntimeError", "x")})
add("_spawn_notice", {"ref": ref()}, {"create_task": "task0"}, notice_tasks=["task1", "task2"])


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")
    low = [fn for fn, n in counts.items() if n < 5]
    if low:
        print(f"WARNING: <5 cases for {low}")


if __name__ == "__main__":
    main()