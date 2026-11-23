#!/usr/bin/env python3
"""Differential oracle for upstream Hermes tools/tool_labels.py (toollabels gate).

Module-level imports are bootstrapped: parse_connector_name is reproduced
verbatim, MCP_TOOL_NAME_PREFIX is "mcp__", and get_tool_verb / get_emoji /
build_tool_preview are injected per case through HOLDER.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def parse_connector_name(name):
    if not isinstance(name, str):
        return None
    parts = name.split("__", 2)
    if len(parts) != 3:
        return None
    prefix, connector, tool = parts
    if prefix != "connectors" or not connector or not tool:
        return None
    return SimpleNamespace(connector=connector, tool=tool)


tools = _mod("tools")
tools.__path__ = []
cpkg = _mod("tools.connectors")
cpkg.__path__ = []
gpkg = _mod("tools.connectors.gateway")
gpkg.__path__ = []
_mod("tools.connectors.gateway.names", parse_connector_name=parse_connector_name)
_mod("tools.mcp_tool_schema", MCP_TOOL_NAME_PREFIX="mcp__")

HOLDER = {"verb": "", "emoji": "", "preview": ""}
agent = _mod("agent")
agent.__path__ = []
_mod("agent.display",
     get_tool_verb=lambda name: HOLDER["verb"],
     build_tool_preview=lambda name, args: HOLDER["preview"])


class _Reg:
    def get_emoji(self, name, default=""):
        return HOLDER["emoji"]


reg = _mod("tools.registry")
reg.registry = _Reg()

SRC = Q + "/tools/tool_labels.py"
spec = importlib.util.spec_from_file_location("toollabels_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["toollabels_live"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "app_title":
            py = live.app_title(r.get("slug", ""))
        elif func == "action_phrase":
            py = live._action_phrase(r.get("tool", ""), r.get("app_slug", ""))
        elif func == "label":
            HOLDER["verb"] = r.get("verb", "")
            HOLDER["emoji"] = r.get("emoji", "")
            HOLDER["preview"] = r.get("preview", "")
            lbl = live.label_for_tool_name(r.get("name", ""), r.get("arguments"))
            py = lbl.as_payload() if lbl is not None else None
        elif func == "bridge":
            py = [l.as_payload() for l in live._bridge_labels(r.get("function_name", ""), r.get("args") or {})]
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE toollabels: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
