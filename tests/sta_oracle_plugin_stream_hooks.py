#!/usr/bin/env python3
"""sta_oracle_plugin_stream_hooks.py — Python twin for
agent/plugin_stream_hooks.py, mirrored by src/agent/plugin_stream_hooks.c.

Consumer callbacks are injected by replacing module._registered_callbacks
(the C port injects the same names via its consumer-registry table).
"""
import json
import sys
import threading

sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from hermes_cli.middleware import OBSERVER_SCHEMA_VERSION
import agent.plugin_stream_hooks as ps

# name -> recorded kwargs list (registration order preserved)
CALLS = {}
_ORDER = []


def _make(name):
    def cb(**kwargs):
        CALLS.setdefault(name, []).append(dict(kwargs))
    cb.__name__ = name
    return cb


def _names_table(hooks):
    table = {h: tuple(_make(n) for n in ns) for h, ns in hooks.items()}
    for ns in table.values():
        for f in ns:
            if f.__name__ not in _ORDER:
                _ORDER.append(f.__name__)
    return table


def out_line(o):
    print(json.dumps(o, ensure_ascii=False, separators=(",", ":")))


def wait_idle():
    with ps._dispatcher_lock:
        ds = list(ps._dispatchers.values())
    for d in ds:
        d.events.join()


def emit_calls(o):
    arr = []
    for name in _ORDER:
        for payload in CALLS.get(name, []):
            arr.append([name, payload])
    o["out"] = arr


def main():
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        fx = json.load(f)
    current = {}  # hook -> callbacks (mutable registry)

    def fake_lookup(hook_name):
        try:
            return tuple(current.get(hook_name, ()))
        except Exception:
            return ()

    ps._registered_callbacks = fake_lookup

    for c in fx.get("ops", []):
        kind = c.get("kind", "")
        o = {}
        if kind in ("setup", "reconfigure"):
            hooks = c.get("hooks")
            if isinstance(hooks, dict):
                for h, names in hooks.items():
                    current[h] = tuple(_make(n) for n in names)
            elif c.get("hook") is not None:
                current[c["hook"]] = tuple(_make(n) for n in c.get("names", []))
            o["out"] = None
        elif kind == "dispatchers_for":
            ready = ps._dispatchers_for(c.get("hook", ""))
            o["out"] = len(ready)
        elif kind == "enqueue":
            q = ps.enqueue_plugin_stream_hook(
                c.get("hook", ""), **(c.get("payload") or {}))
            wait_idle()
            o["out"] = bool(q)
        elif kind == "enqueue_after_shutdown":
            q = ps.enqueue_plugin_stream_hook(c.get("hook", ""))
            o["out"] = bool(q)
        elif kind == "wait_idle":
            wait_idle()
            o["out"] = None
        elif kind == "calls":
            emit_calls(o)
        elif kind == "has_stream_observer_hooks":
            o["out"] = ps.has_stream_observer_hooks()
        elif kind == "reasoning_enabled":
            o["out"] = ps.stream_reasoning_deltas_enabled()
        elif kind == "has_reasoning":
            o["out"] = ps.has_reasoning_stream_observer_hooks()
        elif kind == "shutdown":
            ps.shutdown_plugin_stream_hook_dispatcher(timeout=1.0)
            o["out"] = None
        out_line(o)


if __name__ == "__main__":
    main()
