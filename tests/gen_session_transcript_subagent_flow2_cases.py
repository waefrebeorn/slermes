#!/usr/bin/env python3
"""Generate tests/fixtures/session_transcript_subagent_flow2.jsonl for the
gateway/session_transcript_subagent.py FLOW port (5 defs, >=5 cases each).

Each case is {"func", "in": {"args": {...}, "io": {...}}}. The C harness and
the oracle both drive from the same "in": the harness feeds its universal seam,
the oracle re-drives the LIVE AST-extracted defs with recording stubs fed the
same scripts. Compared: {"ret", "trace"} where trace is an ordered list of
{"name","args","ret"} seam events.

Ported funcs (5):
  unavailable, subagent_transcript, _child, roster (nested in _child), _read.

Seam registry (name -> live call, all scripted through in.io):
  authorize                       authorize(connection, ref, params, 'session:read')
  to_thread                       asyncio.to_thread(_read, authority, live, ref,
                                  binding, held, limit, inline_images)
                                  (ret is the final transcript dict / raise)
  owner_scope                     owner_scope(authority)               (CM factory)
  owner_scope.__enter__           cm.__enter__()
  owner_scope.__exit__            cm.__exit__(exc)
  authority_persistence           authority_persistence(authority)
  child                           _child(authority, live, ref, subagent_id)
                                  (ret = [child, running])
  delegated_child_lineage_ids     store.lineage.delegated_child_lineage_ids(child)
  read_transcript                 read_transcript(store, child, lineage_ids,
                                  binding, held, limit, profile_home=...,
                                  inline_images=...)
  locked_projection               locked_projection(authority, live, ref, roster)
                                  (ret = [records, target])
  require_current_route           _require_current_route(authority, ref)
  owner_objects                   _owner_objects(authority, ref, include_fallback_evicted=True)
  physical_target                 physical_target(authority, ref)
  compression_lineage             _compression_lineage(store, target)
  stored_child_session            stored_child_session(store, subagent_id, lineage)

Domain notes:
- ASCII corpus. Doubles (connection/ref/authority/live/agent/store/record) are
  plain dicts; the oracle materializes them to SimpleNamespace so getattr reads
  agree. authority always carries sessions + profile_id when read; binding is a
  list with >=2 elements; store carries a "lineage" dict when the read path runs.
- Scripted seams: a plain value reused, a bare list consumed per call (last
  value does NOT repeat), or {"__script__": true, "ret"/"raise"}. A bare
  {"__raise__","message"} dict also raises. Missing/exhausted script is a
  fail-closed RuntimeError on both sides.
- owner_scope is a context manager: a __enter__ raise skips the body; a __exit__
  raise replaces a pending body raise; a truthy __exit__ result suppresses it
  (the body-raise case is never suppressed in this corpus: _read would read an
  unbound `page`). A failed owner_scope factory / __enter__ skips body + exit.
- Full builtin exception hierarchy decide any Exception lineage; scripted kinds
  are plain-message classes (KeyError excluded: str(KeyError(k)) == repr(k)).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "session_transcript_subagent_flow2.jsonl")

CASES = []


def add(func, args, io=None, **extra):
    inp = {"args": args, "io": io if io is not None else {}}
    inp.update(extra)
    CASES.append({"func": func, "in": inp})


def rs(type_, message=""):
    return {"__script__": True, "raise": {"type": type_, "message": message}}


def bare(type_, message=""):
    return {"__raise__": type_, "message": message}


# ── shared doubles ──────────────────────────────────────────────────────────
AUTH = {"sessions": {"sess-A": {"token": "LIVE"},
                     "sess-B": {"token": "B"}}, "profile_id": "prof1"}
CONN = {"authority": AUTH}
REF_A = {"session_id": "sess-A"}
REF_MISS = {"session_id": "missing"}
LIVE = {"t": 1}
BIND = ["b0", "sub-1"]
PAGE = {"session_id": "child-live", "rows": [{"role": "user", "content": "hi"}],
        "next_cursor": None, "reset": False, "extra": "v"}
STORE = {"lineage": {"kind": "main"}, "name": "store1"}
LINEAGE = ["c1", "c2"]
TARGET = {"session_key": "parent:1"}

CHILD_ARGS = {"authority": AUTH, "live": LIVE, "ref": REF_A, "subagent_id": "sub-1"}
ROSTER_ARGS = {"authority": AUTH, "ref": REF_A}
READ_ARGS = {"authority": AUTH, "live": LIVE, "ref": REF_A, "binding": BIND,
             "held": {}, "limit": 50, "inline_images": False}


def ret(v):
    """Wrap a list-shaped VALUE so the seam dispatcher doesn't read it as a
    per-call queue (bare arrays are consumed element-by-element). A raise spec
    is already a __script__ wrapper and passes through untouched."""
    if isinstance(v, dict) and v.get("__script__") is True:
        return v
    return {"__script__": True, "ret": v}


def child_io(records, target=TARGET, stored="child-stored", store=STORE, lin=LINEAGE):
    return {"locked_projection": ret([records, target]),
            "authority_persistence": store,
            "compression_lineage": ret(lin),
            "stored_child_session": stored}


# ── unavailable (6) ─────────────────────────────────────────────────────────
add("unavailable", {"session_id": "s1", "subagent_id": "a1", "running": True})
add("unavailable", {"session_id": "", "subagent_id": "", "running": False})
add("unavailable", {"session_id": None, "subagent_id": None, "running": False})
add("unavailable", {"session_id": "parent:1", "subagent_id": "child:2", "running": True})
add("unavailable", {"session_id": "s1", "subagent_id": "a1", "running": None})
add("unavailable", {"session_id": 42, "subagent_id": 7, "running": True})

# ── subagent_transcript (8) ─────────────────────────────────────────────────
def st_args(ref=REF_A, params=None, binding=BIND):
    return {"connection": CONN, "ref": ref, "params": params or {"q": "x"},
            "live": LIVE, "binding": binding, "held": {}, "limit": 50,
            "inline_images": False}


add("subagent_transcript", st_args(), {"authorize": None, "to_thread": PAGE})
add("subagent_transcript", st_args(ref=REF_MISS), {})                      # not_found
add("subagent_transcript", st_args(), {"authorize": rs("AuthorityRefusal", "denied"),
                                       "to_thread": PAGE})
add("subagent_transcript", st_args(), {"authorize": rs("SystemExit", "bye"),
                                       "to_thread": PAGE})
add("subagent_transcript", st_args(), {"authorize": None,
                                       "to_thread": rs("RuntimeError", "thread fail")})
add("subagent_transcript", st_args(), {})                                  # no script for authorize
add("subagent_transcript", st_args(ref={"session_id": 7}), {"authorize": None, "to_thread": PAGE})
add("subagent_transcript", st_args(), {"authorize": None, "to_thread": None})

# ── _child (10) ─────────────────────────────────────────────────────────────
REC_LIVE = {"subagent_id": "sub-1", "agent": {"session_id": "child-live", "kind": "delegate"}}
REC_NOSESS = {"subagent_id": "sub-1", "agent": {"kind": "delegate"}}
REC_EMPTY = {"subagent_id": "sub-1", "agent": {"session_id": ""}}
REC_INT = {"subagent_id": "sub-1", "agent": {"session_id": 123}}
REC_NULLAGENT = {"subagent_id": "sub-1", "agent": None}

add("_child", CHILD_ARGS, child_io([REC_LIVE]))                              # live child, early return
add("_child", CHILD_ARGS, child_io([REC_NOSESS]))                            # fallback -> stored
add("_child", CHILD_ARGS, child_io([]))                                      # no record -> stored, running False
add("_child", CHILD_ARGS, child_io([REC_EMPTY], stored=None))                # '' -> fallback, null child
add("_child", CHILD_ARGS, child_io([REC_INT]))                               # int child -> fallback
add("_child", CHILD_ARGS, child_io([REC_NULLAGENT], stored="child-stored"))  # null agent -> fallback
add("_child", CHILD_ARGS, child_io([REC_LIVE, REC_LIVE]))                    # duplicate -> first match
add("_child", CHILD_ARGS, {**child_io([REC_LIVE]),
                           "locked_projection": rs("KeyErrorLite", "lock busy")})
add("_child", CHILD_ARGS, {**child_io([REC_NOSESS]),
                           "authority_persistence": rs("AuthorityRefusal", "no store")})
add("_child", CHILD_ARGS, {**child_io([REC_NOSESS]),
                           "stored_child_session": rs("RuntimeError", "store fail")})

# ── roster (7) ──────────────────────────────────────────────────────────────
def roster_io(records, target=TARGET):
    return {"require_current_route": None,
            "owner_objects": ret([None, records]),
            "physical_target": target}


add("roster", ROSTER_ARGS, roster_io([REC_LIVE]))
add("roster", ROSTER_ARGS, roster_io([]))
add("roster", ROSTER_ARGS, roster_io(None))
add("roster", ROSTER_ARGS, {**roster_io([REC_LIVE]),
                            "require_current_route": rs("AuthorityRefusal", "bad route")})
add("roster", ROSTER_ARGS, {**roster_io([REC_LIVE]),
                            "owner_objects": rs("RuntimeError", "owner fail")})
add("roster", ROSTER_ARGS, {**roster_io([REC_LIVE]),
                            "physical_target": rs("RuntimeError", "target fail")})
add("roster", ROSTER_ARGS, roster_io({"x": 1}, target=None))

# ── _read (11) ──────────────────────────────────────────────────────────────
_READ_UNSET = object()


def read_io(child=_READ_UNSET, running=True, page=PAGE, store=STORE, lin=LINEAGE, **over):
    ch = "child-live" if child is _READ_UNSET else child
    io = {"owner_scope": {"cm": 1},
          "owner_scope.__enter__": None,
          "owner_scope.__exit__": False,
          "child": ret([ch, running]),
          "authority_persistence": store,
          "delegated_child_lineage_ids": ret(lin),
          "read_transcript": page}
    io.update(over)
    return io


add("_read", READ_ARGS, read_io())                                           # normal
add("_read", READ_ARGS, read_io(child=None, running=False))                  # unavailable path
add("_read", READ_ARGS, read_io(child=None, running=True))                   # unavailable, running True
add("_read", READ_ARGS, read_io(page=rs("RuntimeError", "read fail")))        # read raises, exit called
add("_read", READ_ARGS, read_io(store=rs("AuthorityRefusal", "no store")))    # store raises
add("_read", READ_ARGS, read_io(lin=rs("RuntimeError", "lineage fail")))      # lineage raises
add("_read", READ_ARGS, read_io(**{"owner_scope.__enter__": rs("RuntimeError", "enter fail")}))
add("_read", READ_ARGS, read_io(**{"owner_scope": rs("RuntimeError", "cm fail")}))
add("_read", READ_ARGS, read_io(**{"owner_scope.__exit__": rs("RuntimeError", "exit fail")}))
add("_read", READ_ARGS, read_io(page={"session_id": "OVERRIDE", "subagent_id": "OVERRIDE",
                                      "available": "OVERRIDE", "rows": [], "keep": "yes"}))
add("_read", READ_ARGS, read_io(child=123))                                  # non-null non-str child


def main():
    counts = {}
    for case in CASES:
        counts[case["func"]] = counts.get(case["func"], 0) + 1
    for func in ("unavailable", "subagent_transcript", "_child", "roster", "_read"):
        assert counts.get(func, 0) >= 5, f"{func}: only {counts.get(func, 0)} cases"
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            line = json.dumps(case, ensure_ascii=True, separators=(",", ":"))
            assert line.isascii(), "non-ASCII fixture"
            f.write(line + "\n")
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for func, n in sorted(counts.items()):
        print(f"  {func}: {n}")


if __name__ == "__main__":
    main()
