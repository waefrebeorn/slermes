#!/usr/bin/env python3
"""Generate tests/fixtures/delegate_progress_flow2.jsonl — the fixture corpus
for the delegate_progress_flow2 FLOW oracle (11 unclaimed defs of
tools/delegate_tool_progress.py, >=5 cases each).

Case shape: {"func": <live name>, "in": {..., "io": {seam: script}, "state": {...}}}
Script semantics (shared with the C harness dispatcher): a single value is
reused for every call; a BARE array is a per-call queue; a list RESULT must be
wrapped {"__script__": true, "ret": [...]}; {"__raise__": kind, "message": m}
raises (real builtin classes; SystemExit/KeyboardInterrupt/GeneratorExit pass
every except Exception). Absent/exhausted scripts fail closed with a RuntimeError
"no script for <seam>" raise spec recorded BEFORE propagation.

Domain (see the oracle header): ASCII text; emoji/box-drawing are UTF-8 hex in C;
_short text ASCII; batch members strings; context str-or-null; session_id /
delegation_id strings-or-ints (no integral floats in str() positions); toolsets
array-or-null.
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "delegate_progress_flow2.jsonl"


def attr(**kw):
    return {"__attr__": dict(kw)}


def cb(seam):
    return {"__callable__": seam}


def wrap(ret):
    return {"__script__": True, "ret": ret}


def rs(kind, msg):
    return {"__raise__": kind, "message": msg}


def relay(goal_label="", task_index=0, task_count=1, spinner=None, parent_cb=None,
          session_ref=None, subagent_id=None, parent_id=None, depth=None, model=None,
          toolsets=None, batch=None, tool_count=0, id_=None, io=None, **extra):
    if session_ref is None:
        session_ref = {}
    if batch is None:
        batch = []
    d = {"__attr__": {
        "task_index": task_index, "task_count": task_count, "goal_label": goal_label,
        "spinner": spinner, "parent_cb": parent_cb, "session_ref": session_ref,
        "subagent_id": subagent_id, "parent_id": parent_id, "depth": depth, "model": model,
        "toolsets": toolsets, "batch": batch, "parent_scope": None, "tool_count": tool_count,
    }}
    if id_ is not None:
        d["__id__"] = id_
    out = {"state": {"relay": d}}
    if io is not None:
        out["io"] = io
    out.update(extra)
    return out


CASES = []


def add(func, inp):
    CASES.append({"func": func, "in": inp})


# ---- _normalize_event (7) ----
add("_normalize_event", {"event_type": "subagent.start"})
add("_normalize_event", {"event_type": "subagent.complete"})
add("_normalize_event", {"event_type": "tool.started"})
add("_normalize_event", {"event_type": "_thinking"})
add("_normalize_event", {"event_type": "reasoning.available"})
add("_normalize_event", {"event_type": "delegate.task_progress"})
add("_normalize_event", {"event_type": "unknown.thing"})
add("_normalize_event", {"event_type": None})
add("_normalize_event", {"event_type": ["x"]})

# ---- child_task_message (6) ----
add("child_task_message", {"goal": "g", "context": None})
add("child_task_message", {"goal": "g", "context": ""})
add("child_task_message", {"goal": "g", "context": "   "})
add("child_task_message", {"goal": "g", "context": "ctx", "io": {"register_stable_prefix": None}})
add("child_task_message", {"goal": "goal text", "context": "  ctx  ", "io": {"register_stable_prefix": "ok"}})
add("child_task_message", {"goal": "g", "context": "ctx", "io": {"register_stable_prefix": rs("RuntimeError", "boom")}})

# ---- _ChildProgressRelay.__init__ (6) ----
add("_ChildProgressRelay.__init__", {"task_index": 0, "goal": "g", "task_count": 1})
add("_ChildProgressRelay.__init__", {"task_index": 3, "goal": "  spaced  ", "task_count": 9,
    "session_ref": {"delegation_id": "d", "session_id": "s"}, "subagent_id": "sa", "parent_id": "pa"})
add("_ChildProgressRelay.__init__", {"task_index": 1, "goal": "g", "task_count": 4,
    "spinner": cb("spinner.print_above"), "parent_cb": cb("parent_cb"),
    "toolsets": ["a", "b"], "depth": 2, "model": "m"})
add("_ChildProgressRelay.__init__", {"task_index": 2, "goal": "", "task_count": 2,
    "session_ref": None, "toolsets": None, "depth": None, "model": None})
add("_ChildProgressRelay.__init__", {"task_index": 0, "goal": "g", "task_count": 1,
    "session_ref": {}, "parent_cb": None})
add("_ChildProgressRelay.__init__", {"task_index": 7, "goal": "final", "task_count": 7,
    "session_ref": {"x": 1}, "toolsets": []})

# ---- _ChildProgressRelay._identity_kwargs (6) ----
add("_ChildProgressRelay._identity_kwargs", relay(task_index=0, task_count=1, goal_label="g", tool_count=0))
add("_ChildProgressRelay._identity_kwargs", relay(task_index=2, task_count=9, goal_label="Goal",
    subagent_id="sa", parent_id="pa", depth=3, model="m", toolsets=["a", "b"], tool_count=4,
    session_ref={"session_id": "sess", "delegation_id": "d"}))
add("_ChildProgressRelay._identity_kwargs", relay(goal_label="x", session_ref={"session_id": ""}))
add("_ChildProgressRelay._identity_kwargs", relay(goal_label="x", toolsets=[], tool_count=5))
add("_ChildProgressRelay._identity_kwargs", relay(goal_label="x", session_ref={"session_id": 123}))
add("_ChildProgressRelay._identity_kwargs", relay(goal_label="x", session_ref={"delegation_id": "d"}, id_="r1"))

# ---- _ChildProgressRelay._prefix (6) ----
add("_ChildProgressRelay._prefix", relay(session_ref={"delegation_id": "d1"}, task_index=2, task_count=9,
    io={"batch_prefix": "set 1 · 3/9"}))
add("_ChildProgressRelay._prefix", relay(task_index=0, task_count=1, io={"batch_prefix": ""}))
add("_ChildProgressRelay._prefix", relay(session_ref={"delegation_id": "d", "_parent_scope": attr(platform="cli")},
    task_index=1, task_count=5, io={"batch_prefix": "set 2"}))
add("_ChildProgressRelay._prefix", relay(session_ref={"delegation_id": "d"}, io={"batch_prefix": rs("ValueError", "bad")}))
add("_ChildProgressRelay._prefix", relay(session_ref={"delegation_id": "d"}, io={"batch_prefix": None}))
add("_ChildProgressRelay._prefix", relay(session_ref={"delegation_id": "d", "session_id": "s"}, io={"batch_prefix": "T"}))

# ---- _ChildProgressRelay._tree_line (7) ----
add("_ChildProgressRelay._tree_line", relay(spinner=attr(print_above=cb("spinner.print_above")),
    io={"prefix": "S1", "spinner.print_above": None}, text="hello"))
add("_ChildProgressRelay._tree_line", relay(spinner=None, text="hello"))
add("_ChildProgressRelay._tree_line", relay(spinner=attr(print_above=cb("spinner.print_above")),
    io={"prefix": "S1", "spinner.print_above": None}, text=""))
add("_ChildProgressRelay._tree_line", relay(spinner=attr(print_above=cb("spinner.print_above")),
    io={"prefix": "P", "spinner.print_above": rs("RuntimeError", "boom"), "logger.debug": None}, text="x"))
add("_ChildProgressRelay._tree_line", relay(spinner=attr(print_above=cb("spinner.print_above")),
    io={"prefix": "P", "spinner.print_above": rs("SystemExit", "bye")}, text="x"))
add("_ChildProgressRelay._tree_line", relay(spinner=attr(), io={"prefix": "P", "logger.debug": None}, text="x"))
add("_ChildProgressRelay._tree_line", relay(spinner=attr(print_above=5), io={"prefix": "P", "logger.debug": None}, text="x"))

# ---- _ChildProgressRelay._flush (6) ----
add("_ChildProgressRelay._flush", relay(parent_cb=cb("parent_cb"), batch=["a", "b"],
    io={"prefix": "S1", "relay": None}))
add("_ChildProgressRelay._flush", relay(parent_cb=cb("parent_cb"), batch=[]))
add("_ChildProgressRelay._flush", relay(parent_cb=None, batch=["a"]))
add("_ChildProgressRelay._flush", relay(parent_cb=cb("parent_cb"), batch=["a"],
    io={"prefix": rs("RuntimeError", "pref")}))
add("_ChildProgressRelay._flush", relay(parent_cb=cb("parent_cb"), batch=["a"],
    io={"prefix": "S1", "relay": rs("ValueError", "x")}))
add("_ChildProgressRelay._flush", relay(parent_cb=cb("parent_cb"), batch=["x", "y", "z"],
    io={"prefix": "", "relay": None}))

# ---- _ChildProgressRelay._on_spawn_requested (6) ----
add("_ChildProgressRelay._on_spawn_requested", relay(goal_label="My Goal", io={"relay": None}))
add("_ChildProgressRelay._on_spawn_requested", relay(goal_label="My Goal",
    preview="P", kwargs={"status": "ok"}, io={"relay": None}))
add("_ChildProgressRelay._on_spawn_requested", relay(goal_label="", io={"relay": None}))
add("_ChildProgressRelay._on_spawn_requested", relay(goal_label="", preview="P", io={"relay": None}))
add("_ChildProgressRelay._on_spawn_requested", relay(goal_label="G", io={"relay": rs("RuntimeError", "boom")}))
add("_ChildProgressRelay._on_spawn_requested", relay(goal_label="G", kwargs={"a": 1, "b": [2]}, io={"relay": None}))

# ---- _ChildProgressRelay._on_text (6) ----
add("_ChildProgressRelay._on_text", relay(preview="hello", io={"relay": None}))
add("_ChildProgressRelay._on_text", relay(preview=None, io={"relay": None}))
add("_ChildProgressRelay._on_text", relay(preview="", io={"relay": None}))
add("_ChildProgressRelay._on_text", relay(preview="line\nnext", io={"relay": None}))
add("_ChildProgressRelay._on_text", relay(preview="x", io={"relay": rs("ValueError", "bad")}))
add("_ChildProgressRelay._on_text", relay(preview="x"))

# ---- _ChildProgressRelay._on_start (6) ----
add("_ChildProgressRelay._on_start", relay(goal_label="Goal", io={"short": "G", "tree_line": None, "relay": None}))
add("_ChildProgressRelay._on_start", relay(goal_label="", preview="P", io={"relay": None}))
add("_ChildProgressRelay._on_start", relay(goal_label="Goal", preview="P", kwargs={"status": "x"},
    io={"short": "Sh", "tree_line": None, "relay": None}))
add("_ChildProgressRelay._on_start", relay(goal_label="Goal", io={"short": rs("RuntimeError", "s")}))
add("_ChildProgressRelay._on_start", relay(goal_label="Goal", io={"short": "G", "tree_line": rs("ValueError", "t")}))
add("_ChildProgressRelay._on_start", relay(goal_label="Goal", io={"short": "G", "tree_line": None, "relay": rs("RuntimeError", "r")}))

# ---- _ChildProgressRelay._on_thinking (6) ----
add("_ChildProgressRelay._on_thinking", relay(preview="think", io={"short": "t", "tree_line": None, "relay": None}))
add("_ChildProgressRelay._on_thinking", relay(tool_name="tool", io={"short": "TO", "tree_line": None, "relay": None}))
add("_ChildProgressRelay._on_thinking", relay(io={"short": "", "tree_line": None, "relay": None}))
add("_ChildProgressRelay._on_thinking", relay(preview="p", tool_name="t", io={"short": "x", "tree_line": None, "relay": None}))
add("_ChildProgressRelay._on_thinking", relay(preview="think", io={"short": rs("RuntimeError", "s")}))
add("_ChildProgressRelay._on_thinking", relay(preview="think", io={"short": "t", "tree_line": rs("ValueError", "tl")}))
add("_ChildProgressRelay._on_thinking", relay(preview="think", io={"short": "t", "tree_line": None, "relay": rs("RuntimeError", "r")}))

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")

from collections import Counter
cnt = Counter(c["func"] for c in CASES)
print("wrote", OUT, "cases:", len(CASES))
for k in sorted(cnt):
    print("  %-48s %d" % (k, cnt[k]))
