#!/usr/bin/env python3
"""Oracle for the tools/process_registry.py pure-function gap ports
(src/tools/process_registry_gaps.c) -- drives the LIVE quarry defs.

Importing tools.process_registry pulls the whole tool stack and instantiates
a registry at module level, so the pure defs are AST-extracted (seed
constants + helpers, provide re/os/json/sys &c. in the namespace) exactly
like tests/oracle_container_boot.py. No third-party deps are needed, so no
venv re-exec guard is required. The C ports' injected dependencies are
honored on the Python side:
  * _watch_event_base mints event_id via uuid.uuid4().hex -- the namespace's
    uuid is pinned to the case's event_id before each call;
  * _owns_event reads event_visible(evt) (bound-scope contextvar) and an
    owns_event callback -- visibility is computed live (no scope is bound:
    local events visible, hosted ones not) and cross-checked against the
    case's "visible" input; the callback is rebuilt from "owns_verdict"
    (null = None, true/false = result, "error" = raises -> fail closed).
"""
import ast
import importlib.util
import json
import os
import sys
import types
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/tools/process_registry.py"
sys.path.insert(0, QUARRY)

WANTED_METHODS = {"_watch_event_base", "_global_watch_event", "_owns_event",
                  "_exit_fields", "_status_head", "_exit_snapshot"}

# ProcessSession dataclass defaults for the fields the pure defs read.
SESSION_DEFAULTS = dict(
    id="", command="", task_id="", owner_task_id="", session_key="",
    sandbox_key="", parent_session_id="", started_at=0.0, exited=False,
    exit_code=None, completion_reason="exited", termination_source="",
    output_buffer="", completion_output_chars=0,
    watcher_platform="", watcher_chat_id="", watcher_user_id="",
    watcher_user_name="", watcher_thread_id="", watcher_message_id="")


def load_sandbox_scope():
    """tools/environments/__init__ imports the backend stack; register a stub
    package and load the REAL sandbox_scope.py (its hermes_scope dependency
    is stdlib-only) so _owns_event's inner import resolves to live code."""
    pkg = types.ModuleType("tools.environments")
    pkg.__path__ = [os.path.join(QUARRY, "tools", "environments")]
    sys.modules.setdefault("tools.environments", pkg)
    name = "tools.environments.sandbox_scope"
    if name in sys.modules:
        return sys.modules[name]
    spec = importlib.util.spec_from_file_location(
        name, os.path.join(QUARRY, "tools", "environments", "sandbox_scope.py"))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


def extract(ss):
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    import re as _re
    import logging as _logging
    import platform as _platform
    import uuid as _uuid
    import time as _time
    import subprocess as _subprocess
    import threading as _threading
    from pathlib import Path
    from typing import Any, Dict, List, Literal, NamedTuple, Optional

    class ProcessRegistry:  # synthetic holder for the extracted staticmethods
        pass

    class ProcessSession:  # annotation seed only; fields ride SimpleNamespace
        pass

    ns = {"re": _re, "os": os, "json": json, "sys": sys,
          "logging": _logging, "platform": _platform, "uuid": _uuid,
          "time": _time, "subprocess": _subprocess, "threading": _threading,
          "Path": Path, "Any": Any, "Dict": Dict, "List": List,
          "Literal": Literal, "NamedTuple": NamedTuple, "Optional": Optional,
          "logger": _logging.getLogger("process_registry"),
          "producer_fields": ss.producer_fields,
          "ProcessRegistry": ProcessRegistry, "ProcessSession": ProcessSession,
          "__name__": "process_registry_oracle"}
    # Module-level constants the defs read (COMPLETION_OUTPUT_CHARS,
    # _WATCHER_ROUTE_KEYS, ...). Assignments calling unseeded helpers fail
    # softly; exec only DEFINES, nothing runs until a case calls a target.
    for node in tree.body:
        if isinstance(node, ast.Assign):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and not node.decorator_list:
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in tree.body:
        if not isinstance(node, ast.ClassDef):
            continue
        if node.name == "ProcessRegistry":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name in WANTED_METHODS:
                    sub.decorator_list = []  # strip @staticmethod
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), SRC, "exec"), ns)
                    setattr(ProcessRegistry, sub.name, staticmethod(ns[sub.name]))
        elif node.name == "ProcessSession":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and sub.name == "__post_init__":
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), SRC, "exec"), ns)
                    ns["session_post_init"] = ns["__post_init__"]
    return ns


def make_session(d, ns):
    s = SimpleNamespace(**SESSION_DEFAULTS)
    for k, v in (d or {}).items():
        setattr(s, k, v)
    ns["session_post_init"](s)  # the live dataclass runs this on construction
    return s


def expected(fn, inp, ns, ss):
    PR = ns["ProcessRegistry"]
    if fn == "process_registry_not_found":
        return ns["_not_found"](inp["session_id"])
    if fn == "process_registry_output_tail":
        return ns["_output_tail"](
            SimpleNamespace(output_buffer=inp["output_buffer"]), inp["n"])
    if fn == "process_registry_completion_output":
        return ns["_completion_output"](make_session(inp.get("session"), ns))
    if fn == "process_registry_exit_fields":
        return PR._exit_fields(make_session(inp.get("session"), ns))
    if fn == "process_registry_status_head":
        return PR._status_head(make_session(inp.get("session"), ns))
    if fn == "process_registry_exit_snapshot":
        return PR._exit_snapshot(make_session(inp.get("session"), ns), inp["status"])
    if fn == "process_registry_global_watch_event":
        return PR._global_watch_event(inp["type"], inp["message"],
                                      **(inp.get("extra") or {}))
    if fn == "process_registry_watch_event_base":
        eid = inp["event_id"]
        ns["uuid"] = SimpleNamespace(uuid4=lambda: SimpleNamespace(hex=eid))
        return PR._watch_event_base(make_session(inp.get("session"), ns))
    if fn == "process_registry_session_post_init":
        s = SimpleNamespace(task_id="", owner_task_id="")
        for k, v in (inp.get("session") or {}).items():
            setattr(s, k, v)
        ns["session_post_init"](s)
        return s.owner_task_id
    if fn == "process_registry_owns_event":
        evt = inp["evt"]
        live_visible = ss.event_visible(evt)
        if bool(live_visible) != bool(inp["visible"]):
            raise ValueError("case 'visible' disagrees with live event_visible")
        v = inp.get("owns_verdict")
        if v is None:
            cb = None
        elif v == "error":
            def cb(e):
                raise RuntimeError("broken ownership check")
        else:
            cb = lambda e, _v=v: _v
        return PR._owns_event(evt, inp["session_key"], cb,
                              inp["is_async_delegation"])
    raise KeyError(fn)


def main():
    ss = load_sandbox_scope()
    ns = extract(ss)
    n = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith('{'):
            continue
        rec = json.loads(line)
        n += 1
        fn = rec["func"]
        try:
            exp = expected(fn, rec["in"], ns, ss)
        except Exception as e:
            mism += 1
            print("PYERR", fn, repr(e))
            continue
        got = rec["out"]
        if got != exp:
            mism += 1
            print("MISMATCH", fn, "got", repr(got), "exp", repr(exp))
    print("ORACLE process_registry: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
