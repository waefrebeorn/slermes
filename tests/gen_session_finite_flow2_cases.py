#!/usr/bin/env python3
"""Generate tests/fixtures/session_finite_flow2.jsonl for the
gateway/session_finite.py FLOW remainder oracle. >=5 cases per ported function
(6 funcs — the module's full unclaimed ceiling).

Each case is {"func", "in": {"args", "state", "io", ...}}. io scripts the
universal seam (a plain value reused, a list consumed per call, or
{"__script__": true, "ret"/"raise"}). The oracle re-drives the LIVE reference
from "in"; the C harness drives the port from the same "in".

Domain notes (shared with the port header):
- ASCII fixture corpus. ContextVar state: finite_turn (default None) /
  unattended_turn (default False); only present keys echo; set+reset restores.
- params: dict-or-null (null -> py3.14 membership TypeError). submitter:
  null/list/str/dict (iterable splat); non-iterable scalars are OUT of the corpus
  (call-splat TypeError carries a module prefix). A wrong element count raises
  the py3.14 call-splat BINDING TypeError (missing/too-many positional args, no
  module prefix), not an unpack ValueError.
- connection/ref/source/actor/entry/policy carriers: dict (domain keys read as
  None when absent) or null/scalar (deterministic AttributeError surface).
  user_id/subject: str-or-null. session_id: str/int/bool/null (KeyError
  str()==repr(key); str sid ASCII without quotes/backslashes). row: dict-or-null;
  payload: dict/null/str/int/bool/list (float excluded).
- A dict CM-factory result is a CM carrier; null/str/int results pass through to
  the live `with` protocol (py3.14 TypeError). Scripted raise kinds are
  plain-message classes (KeyError excluded from scripted raises); bare raise
  dicts keep the uniform {"__raise__","message"} shape. AuthorityRefusal is the
  live refusal admit_finite raises.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "session_finite_flow2.jsonl")

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


# ── attribute-double helpers ──
def conn(uid="u1", subject="u1", owner=True, runner="r", sid="s1", source=None):
    if source is None:
        source = {"user_id": uid}
    return {"native_owner": owner,
            "authority": {"sessions": {sid: {"source": source}}, "runner": runner},
            "actor": {"subject": subject}}


def ref(sid="s1"):
    return {"session_id": sid}


FACTS = ["surface_turn_scope", "display_turn_scope", "submission_turn_scope", "cli_turn_scope"]


def exec_io(body=None):
    io = {}
    for f in FACTS:
        io[f] = {"c": 1}
        io[f + ".__enter__"] = None
        io[f + ".__exit__"] = False
    io["execute_admission"] = body
    return io


def row(payload=None, aid="a1", rid=None):
    r = {"admission_id": aid}
    if payload is not None:
        r["payload"] = payload
    if rid is not None:
        r["request_id"] = rid
    return r


# ═══════════════════════ finite_clarify_reply (6) ═══════════════════════
add("finite_clarify_reply", {}, {})
add("finite_clarify_reply", {}, {}, state={"finite_turn": "x"})
add("finite_clarify_reply", {}, {}, state={"unattended_turn": True})
add("finite_clarify_reply", {}, {}, state={"finite_turn": None, "unattended_turn": False})
add("finite_clarify_reply", {}, no_io=True)
add("finite_clarify_reply", {}, {}, state={"finite_turn": {"k": 1}})

# ═══════════════════════ unattended_turn (7) ═══════════════════════
add("unattended_turn", {}, {})
add("unattended_turn", {}, {}, state={"unattended_turn": True})
add("unattended_turn", {}, {}, state={"unattended_turn": False})
add("unattended_turn", {}, {}, state={"unattended_turn": None})
add("unattended_turn", {}, {}, state={"unattended_turn": "yes"})
add("unattended_turn", {}, {}, state={"unattended_turn": 0})
add("unattended_turn", {}, {}, state={"finite_turn": "x"})

# ═══════════════════════ finite_turn_scope (12) ═══════════════════════
add("finite_turn_scope", {"finite": True}, {"body": None})
add("finite_turn_scope", {"finite": True, "unattended": True}, {"body": None})
add("finite_turn_scope", {"finite": False, "unattended": True}, {"body": None})
add("finite_turn_scope", {"finite": None, "unattended": True}, {"body": None})
add("finite_turn_scope", {"finite": "x", "unattended": "y"}, {"body": None})
add("finite_turn_scope", {"finite": 0, "unattended": True}, {"body": None})
add("finite_turn_scope", {"finite": True}, {"body": rs("RuntimeError", "boom")})
add("finite_turn_scope", {"finite": True}, {"body": rs("SystemExit", "1")})
add("finite_turn_scope", {"finite": True}, no_io=True)
add("finite_turn_scope", {"finite": True}, {})
add("finite_turn_scope", {"finite": True}, {"body": {"__script__": True, "ret": {"done": 1}}},
    state={"finite_turn": "prev", "unattended_turn": True})
add("finite_turn_scope", {"finite": True, "unattended": True}, {"body": [None]},
    state={"finite_turn": "p", "unattended_turn": False})

# ═══════════════════════ admit_finite (20) ═══════════════════════
add("admit_finite", {"params": {}}, {})
add("admit_finite", {"params": {"finite": True}}, {})
add("admit_finite", {"params": {"finite": False}}, {})
add("admit_finite", {"params": {"finite": "x"}}, {})
add("admit_finite", {"params": {"finite": 1}}, {})
add("admit_finite", {"params": {"unattended": True, "finite": True}}, {})
add("admit_finite", {"params": {"unattended": True, "finite": False}}, {})
add("admit_finite", {"params": {"unattended": True}}, {})
add("admit_finite", {"params": {"unattended": "x"}}, {})
add("admit_finite", {"params": {"unattended": False, "finite": True}}, {})
add("admit_finite", {"params": None}, {})
add("admit_finite", {"params": {"unattended": True, "finite": True},
                     "submitter": [conn(), ref()]}, {"policy_for_source": {"platform": "cli"}})
add("admit_finite", {"params": {"unattended": True, "finite": True},
                     "submitter": [conn(), ref()]}, {"policy_for_source": {"platform": "web"}})
add("admit_finite", {"params": {"unattended": True, "finite": True},
                     "submitter": [conn(), ref()]}, {"policy_for_source": None})
add("admit_finite", {"params": {"unattended": True, "finite": True},
                     "submitter": [conn(), ref()]}, {"policy_for_source": rs("RuntimeError", "p")})
add("admit_finite", {"params": {"unattended": True, "finite": True}, "submitter": "ab"}, {})
add("admit_finite", {"params": {"unattended": True, "finite": True}, "submitter": [conn()]}, {})
add("admit_finite", {"params": {"unattended": True, "finite": True},
                     "submitter": [conn(), ref(), {"x": 1}]}, {})
add("admit_finite", {"params": {"unattended": True, "finite": True},
                     "submitter": {"a": 1, "b": 2}}, {})
add("admit_finite", {"params": {"unattended": True, "finite": True}, "submitter": None}, {})
add("admit_finite", {"params": {"unattended": True, "finite": True},
                     "submitter": [conn(), ref()]}, {})  # fail-closed: no policy script
add("admit_finite", {"params": {"unattended": True, "finite": True}, "submitter": []}, {})
add("admit_finite", {"params": {"unattended": True, "finite": True}, "submitter": "a"}, {})
add("admit_finite", {"params": {"unattended": True, "finite": True}, "submitter": "abc"}, {})
add("admit_finite", {"params": {"unattended": True, "finite": True}, "submitter": {"a": 1}}, {})
add("admit_finite", {"params": {"unattended": True, "finite": True},
                     "submitter": {"a": 1, "b": 2, "c": 3}}, {})

# ═══════════════════════ _may_run_unattended (22) ═══════════════════════
add("_may_run_unattended", {"connection": conn(), "ref": ref()}, {"policy_for_source": {"platform": "cli"}})
add("_may_run_unattended", {"connection": conn(), "ref": ref()}, {"policy_for_source": {"platform": "web"}})
add("_may_run_unattended", {"connection": conn(), "ref": ref()}, {"policy_for_source": None})
add("_may_run_unattended", {"connection": conn(owner=False), "ref": ref()}, {})
add("_may_run_unattended", {"connection": conn(source=None), "ref": ref()}, {})
add("_may_run_unattended", {"connection": conn(uid="u1", subject="u2"), "ref": ref()}, {})
add("_may_run_unattended", {"connection": conn(), "ref": ref("missing")}, {})
add("_may_run_unattended", {"connection": None, "ref": ref()}, {})
add("_may_run_unattended", {"connection": conn(), "ref": None}, {})
add("_may_run_unattended", {"connection": {"native_owner": True, "authority": None, "actor": {"subject": "u1"}},
                            "ref": ref()}, {})
add("_may_run_unattended", {"connection": {"native_owner": True, "authority": {"sessions": None, "runner": "r"},
                                           "actor": {"subject": "u1"}}, "ref": ref()}, {})
add("_may_run_unattended", {"connection": {"native_owner": True,
                                           "authority": {"sessions": {"s1": None}, "runner": "r"},
                                           "actor": {"subject": "u1"}}, "ref": ref()}, {})
add("_may_run_unattended", {"connection": conn(source="x"), "ref": ref()}, {})
add("_may_run_unattended", {"connection": {"native_owner": True,
                                           "authority": {"sessions": {"s1": {"source": {"user_id": "u1"}}}, "runner": "r"},
                                           "actor": None}, "ref": ref()}, {})
add("_may_run_unattended", {"connection": conn(), "ref": ref()}, {"policy_for_source": "x"})
add("_may_run_unattended", {"connection": conn(), "ref": ref()}, {"policy_for_source": rs("RuntimeError", "boom")})
add("_may_run_unattended", {"connection": conn(), "ref": ref()}, {"policy_for_source": rs("SystemExit", "1")})
add("_may_run_unattended", {"connection": conn(), "ref": ref()}, {}, import_fail=["gateway.session_policy"])
add("_may_run_unattended", {"connection": conn(), "ref": ref()}, {})  # fail-closed
add("_may_run_unattended", {"connection": "x", "ref": ref()}, {})
add("_may_run_unattended", {"connection": conn(), "ref": {"session_id": 5}}, {})
add("_may_run_unattended", {"connection": conn(uid=None, subject=None), "ref": ref()},
    {"policy_for_source": {"platform": "cli"}})

# ═══════════════════════ execute_finite_admission (24) ═══════════════════════
P = {"finite": True, "unattended": True}
add("execute_finite_admission", {"authority": {"a": 1}, "ref": ref(), "row": row(P)}, exec_io({"ok": True}))
add("execute_finite_admission", {"authority": {"a": 1}, "ref": ref(), "row": row(P)},
    exec_io(rs("RuntimeError", "body")))
add("execute_finite_admission", {"authority": {"a": 1}, "ref": ref(), "row": row(P)},
    exec_io(rs("SystemExit", "1")))
# surface factory raise -> only finite reset
io = exec_io({"ok": True}); io["surface_turn_scope"] = rs("RuntimeError", "sf")
add("execute_finite_admission", {"authority": None, "ref": None, "row": row(P)}, io)
# surface factory null (non-carrier) -> TypeError
io = exec_io({"ok": True}); io["surface_turn_scope"] = None
add("execute_finite_admission", {"authority": None, "ref": None, "row": row(P)}, io)
# surface factory str (non-carrier)
io = exec_io({"ok": True}); io["surface_turn_scope"] = "x"
add("execute_finite_admission", {"authority": None, "ref": None, "row": row(P)}, io)
# surface enter raise
io = exec_io({"ok": True}); io["surface_turn_scope.__enter__"] = rs("RuntimeError", "en")
add("execute_finite_admission", {"authority": None, "ref": None, "row": row(P)}, io)
# admission_id missing -> KeyError after surface enter
io = exec_io({"ok": True})
add("execute_finite_admission", {"authority": None, "ref": None, "row": {"payload": P}}, io)
# payload missing -> KeyError 'payload' (empty trace)
add("execute_finite_admission", {"authority": None, "ref": None, "row": {"admission_id": "a1"}}, exec_io())
# row null -> TypeError not subscriptable
add("execute_finite_admission", {"authority": None, "ref": None, "row": None}, exec_io())
# payload null -> AttributeError NoneType get
add("execute_finite_admission", {"authority": None, "ref": None, "row": row(None)}, exec_io())
# payload scalar str -> AttributeError str get
add("execute_finite_admission", {"authority": None, "ref": None, "row": row("x")}, exec_io())
# display factory raise -> surface exit(exc), finite reset
io = exec_io({"ok": True}); io["display_turn_scope"] = rs("RuntimeError", "df")
add("execute_finite_admission", {"authority": None, "ref": None, "row": row(P)}, io)
# display enter raise
io = exec_io({"ok": True}); io["display_turn_scope.__enter__"] = rs("ValueError", "de")
add("execute_finite_admission", {"authority": None, "ref": None, "row": row(P)}, io)
# cli factory raise -> submission/display/surface exits see exc
io = exec_io({"ok": True}); io["cli_turn_scope"] = rs("RuntimeError", "cf")
add("execute_finite_admission", {"authority": None, "ref": None, "row": row(P)}, io)
# cli exit raise on SUCCESS -> replaces ret, outer exits see exc
io = exec_io({"ok": True}); io["cli_turn_scope.__exit__"] = rs("RuntimeError", "cx")
add("execute_finite_admission", {"authority": None, "ref": None, "row": row(P)}, io)
# cli exit suppress (true) on body raise -> outer exits see exc=None, ret null
io = exec_io(rs("RuntimeError", "body")); io["cli_turn_scope.__exit__"] = True
add("execute_finite_admission", {"authority": None, "ref": None, "row": row(P)}, io)
# import gates (first / last)
add("execute_finite_admission", {"authority": None, "ref": None, "row": row(P)}, exec_io(),
    import_fail=["gateway.session_ingress"])
add("execute_finite_admission", {"authority": None, "ref": None, "row": row(P)}, exec_io(),
    import_fail=["gateway.session_cli_turn"])
# fail-closed: no surface script
add("execute_finite_admission", {"authority": None, "ref": None, "row": row(P)}, {})
# no io channel
add("execute_finite_admission", {"authority": None, "ref": None, "row": row(P)}, no_io=True)
# state echo restored
add("execute_finite_admission", {"authority": None, "ref": None, "row": row(P)}, exec_io({"ok": True}),
    state={"finite_turn": "prev", "unattended_turn": True})
# request_id present + surface_v1/display_v1/cli_turn_v1 args
io = exec_io({"ok": True})
add("execute_finite_admission", {"authority": {"a": 1}, "ref": ref(),
                                 "row": row({"finite": False, "surface_v1": "s", "display_v1": "d",
                                             "cli_turn_v1": "c"}, rid="r1")}, io)
# exit queue (per-call) on surface exit
io = exec_io({"ok": True}); io["surface_turn_scope.__exit__"] = [False]
add("execute_finite_admission", {"authority": None, "ref": None, "row": row(P)}, io)


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-func count report
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")
    low = [fn for fn, n in counts.items() if n < 5]
    if low:
        raise SystemExit(f"FATAL: <5 cases for {low}")


if __name__ == "__main__":
    main()
