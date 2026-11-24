#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/a2a/adapter.py, gate p3.

The JSON-RPC dispatch path's pure half.  ``A2AAdapter`` is built with
``__new__``; the task store is a stand-in that always misses (and, for the
inline push config, records the call), which is the seam -- no task is ever
created, no HTTP response is written.

Compared values: the ``_METHODS`` table, ``_find_task``'s not-found envelope,
``_push_config_op`` validation, ``_rpc_push_config_create``'s invalid-params
envelope, ``_register_inline_push``'s URL extraction, the
``_note_forwarded_input`` / ``_forget_forwarded_input`` admission LRU and
``get_chat_info``.
"""
import asyncio
import json
import os
import sys
from collections import OrderedDict
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_adapter_0e0bda_p3"

# A login shell can start without a Windows home; the live package resolves
# hermes_home at import time, so anchor it on the quarry tree's parent.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(QUARRY.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.platforms.a2a import adapter as live  # noqa: E402

WANTED = {"method_lookup", "find_task_error", "push_op_error", "push_create_error",
          "inline_push_url", "note_forwarded", "forget_forwarded", "chat_info"}

ROOT_AGENT = {"slug": "", "path": "", "tenant": "", "profile": "default", "local": True,
              "name": "hermes-test", "description": "d", "advertised_toolsets": [],
              "timeout": 300}


def bare(localhost_only=False):
    a = live.A2AAdapter.__new__(live.A2AAdapter)
    a.host, a.port = "127.0.0.1", live._DEFAULT_PORT
    a.agent_name = "hermes-test"
    a._agents = {"": dict(ROOT_AGENT)}
    a._advertised_toolsets = []
    a._security_context = SimpleNamespace(localhost_only=lambda: localhost_only)
    return a


def method_lookup(r):
    entry = live._METHODS.get(r.get("method"))
    if entry is None:
        return None
    handler, is_v1 = entry
    return {"handler": handler, "isV1": bool(is_v1)}


def find_task_error(r):
    a = bare()
    a.tasks = SimpleNamespace(get=lambda *args: None)
    _tid, _rec, error = a._find_task(r.get("req_id"), r.get("params") or {}, a._agents[""])
    return error


def push_op_error(r):
    a = bare()
    return a._push_config_op(r.get("req_id"), r.get("params") or {}, a._agents[""],
                             lambda *args: None, lambda found: found)


def push_create_error(r):
    a = bare()
    a.tasks = SimpleNamespace(set_push_config=lambda *args: None)
    return a._rpc_push_config_create(r.get("req_id"), r.get("params") or {}, a._agents[""])


def inline_push_url(r):
    a = bare()
    seen = []
    a.tasks = SimpleNamespace(set_push_config=lambda *args: seen.append(args))
    a._register_inline_push("task-fixture", r.get("params") or {}, a._agents[""])
    return seen[0][1] if seen else ""


def _lru(r):
    a = bare()
    a._forwarded_inputs = OrderedDict((str(k), None) for k in (r.get("order") or []))
    a._pending_lock = __import__("threading").Lock()
    if r.get("max") is not None:
        a._MAX_FORWARDED_INPUTS = int(r.get("max"))
    return a


def note_forwarded(r):
    a = _lru(r)
    seen = a._note_forwarded_input(r.get("input_id"))
    return {"seen": bool(seen), "order": list(a._forwarded_inputs)}


def forget_forwarded(r):
    a = _lru(r)
    a._forget_forwarded_input(r.get("input_id"))
    return {"order": list(a._forwarded_inputs)}


def chat_info(r):
    return asyncio.run(bare().get_chat_info(r.get("chat_id")))


HANDLERS = {
    "method_lookup": method_lookup,
    "find_task_error": find_task_error,
    "push_op_error": push_op_error,
    "push_create_error": push_create_error,
    "inline_push_url": inline_push_url,
    "note_forwarded": note_forwarded,
    "forget_forwarded": forget_forwarded,
    "chat_info": chat_info,
}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = HANDLERS[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
