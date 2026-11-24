#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_activation_live.py (gate w26_pluginsact_d4fbe5).

The live functions are called whole: ``live_notice`` reads only the activation
mapping it is handed (the ``connect_plugin_mcp`` / ``plugin_skills`` seams have
already run upstream), and ``_tool_prefix`` reads the module-level MCP name
prefix plus ``sanitize_mcp_name_component``. Nothing is connected and no plugin
manager is loaded; the fixture rows ARE the connected facts.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_pluginsact_d4fbe5"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import plugins_activation_live as live  # noqa: E402
from tools.mcp_tool_schema import MCP_TOOL_NAME_PREFIX  # noqa: E402

WANTED = {"live_notice", "tool_prefix", "constants"}


def live_notice(req):
    return live.live_notice(req)


def tool_prefix(req):
    return live._tool_prefix(req.get("server"))


def constants():
    return {
        "tool_name_prefix": MCP_TOOL_NAME_PREFIX,
        "utility_suffixes": list(live._utility_suffixes()),
    }


def expected(r):
    f = r.get("func")
    if f == "live_notice":
        return live_notice(r)
    if f == "tool_prefix":
        return tool_prefix(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


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
            py = expected(r)
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
