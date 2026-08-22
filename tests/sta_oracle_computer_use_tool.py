#!/usr/bin/env python3
"""sta_oracle_computer_use_tool.py — Python twin for src/tools/computer_use_tool.c.

Imports the LIVE tools/computer_use_tool.py shim (its import performs the
registry.register) plus the real tool.py functions, and answers the same ops
as the C harness.
"""
import json
import os
import sys

def _import_live():
    """Import the live tool modules. The runner's isolated HOME symlinks
    ~/.hermes/hermes-agent at the dev tree, but that tree's hermes_constants
    may lack symbols newer ports import — fall back to the real developer
    install when the symlinked copy fails to import."""
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
            from tools.registry import registry  # noqa: E402
            import tools.computer_use_tool as shim  # noqa: E402,F401
            from tools.computer_use.tool import (  # noqa: E402
                check_computer_use_requirements,
                release_computer_use_session,
            )
            return registry, shim, check_computer_use_requirements, release_computer_use_session
        except Exception as e:  # noqa: BLE001
            last = e
            # Purge partially-initialized modules so the next candidate's
            # packages are imported fresh (parent `tools` stays cached).
            for m in [k for k in list(sys.modules)
                      if k == "tools" or k.startswith("tools.")
                      or k.startswith("hermes") or k == "tools_computer_use_tool"
                      or k == "repetition_guard_live"]:
                del sys.modules[m]
    raise last if last else RuntimeError("no live root found")


registry, _shim, check_computer_use_requirements, release_computer_use_session = _import_live()


def _info():
    entry = registry._tools.get("computer_use")
    if entry is None:
        return {"registered": False}
    schema = entry.schema if isinstance(entry.schema, dict) else {}
    params = schema.get("parameters") if isinstance(schema, dict) else {}
    schema_ok = (
        isinstance(schema, dict)
        and schema.get("name") == "computer_use"
        and isinstance(params, dict)
        and params.get("type") == "object"
    )
    return {
        "registered": True,
        "name": entry.name,
        "toolset": entry.toolset,
        "description": entry.description,
        "schema_ok": schema_ok,
    }


def main():
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        kind = c.get("kind")
        if kind == "register":
            # The shim registers at import time; re-import is a no-op, so the
            # registry state IS the registration result.
            out = _info()
        elif kind == "requirements":
            out = bool(check_computer_use_requirements())
        elif kind == "release":
            out = bool(release_computer_use_session(c.get("session_id", "")))
        else:
            out = None
        print(json.dumps({"out": out}, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main()
