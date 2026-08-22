#!/usr/bin/env python3
"""sta_oracle_read_preview_tool.py — Python twin for src/tools/read_preview_tool.c."""
import json
import os
import sys


def _import_live():
    candidates = [
        os.environ.get("HERMES_LIVE_ROOT"),
        os.path.join(os.path.expanduser("~"), ".hermes/hermes-agent"),
        "/home/wubu/.hermes/hermes-agent",
    ]
    last = None
    for root in candidates:
        if not root or not os.path.isdir(os.path.join(root, "tools")):
            continue
        while root in sys.path:
            sys.path.remove(root)
        sys.path.insert(0, root)
        try:
            from tools.registry import registry
            import tools.read_preview_tool as shim
            return registry, shim
        except Exception as e:  # noqa: BLE001
            last = e
            for m in [k for k in list(sys.modules)
                      if k == "tools" or k.startswith("tools.") or k.startswith("hermes")]:
                del sys.modules[m]
    raise last if last else RuntimeError("no live root found")


registry, shim = _import_live()


class _FakeExc(Exception):
    pass


def make_callback(mode, payload, seen):
    if mode == "null":
        return None
    if mode == "error":
        def cb(**window):
            raise _FakeExc(payload)
        return cb

    def cb(**window):
        seen.update(window)
        return payload
    return cb


def main():
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        kind = c.get("kind")
        if kind == "read_preview":
            seen = {}
            cb = make_callback(c.get("callback", "null"), c.get("payload", ""), seen)
            kwargs = {}
            if c.get("start") is not None:
                kwargs["start"] = c["start"]
            if c.get("count") is not None:
                kwargs["count"] = c["count"]
            out = json.loads(shim.read_preview_tool(callback=cb, **kwargs))
            cw = {
                "has_start": "start" in seen,
                "start": seen.get("start", 0),
                "has_count": "count" in seen,
                "count": seen.get("count", 0),
            }
            out = {"result": out, "callback_window": cw}
        elif kind == "register":
            entry = registry._tools.get("read_preview")
            if entry is None:
                out = {"registered": False}
            else:
                out = {
                    "registered": True,
                    "name": entry.name,
                    "toolset": entry.toolset,
                    "description": entry.description,
                }
        else:
            out = None
        print(json.dumps({"out": out}, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main()
