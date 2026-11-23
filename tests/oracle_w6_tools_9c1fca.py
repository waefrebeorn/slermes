#!/usr/bin/env python3
"""Differential oracle for acp_adapter/tools.py (gate w6_tools_9c1fca)."""
import importlib.util, json, sys, types
sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# acp_adapter.tools imports acp + agent.display at module load. The pure helpers
# exercised here never touch them, so provide minimal stand-ins.
acp = types.ModuleType("acp")
acp.tool_content = lambda x: x
acp.text_block = lambda x: x
sys.modules["acp"] = acp
schema = types.ModuleType("acp.schema")
for _n in ("ToolCallLocation", "ToolCallProgress", "ToolCallStart", "ToolKind"):
    setattr(schema, _n, type(_n, (object,), {}))
sys.modules["acp.schema"] = schema
ad = types.ModuleType("agent.display")
ad.build_tool_preview = lambda *a, **k: "PREVIEW"
sys.modules["agent.display"] = ad

SRC = "C:/Users/eman5/hermes-agent/acp_adapter/tools.py"
spec = importlib.util.spec_from_file_location("w6_tools_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w6_tools_live"] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "tool_kind":
        return live.get_tool_kind(r.get("tool_name", ""))
    if f == "plural":
        return live._plural(int(r.get("count", 0)), r.get("word", ""), r.get("suffix", "s"))
    if f == "nonempty":
        return live._nonempty(r.get("result"))
    if f == "failure":
        return live._failure(r.get("data", {}), r.get("prefix", ""))
    if f == "truncate_text":
        return live._truncate_text(r.get("text", ""), int(r.get("limit", 5000)))
    if f == "fenced_text":
        return live._fenced_text(r.get("text", ""), r.get("language", ""))
    if f == "json_loads_maybe":
        return live._json_loads_maybe(r.get("value"))
    if f == "coerce_args":
        return live.coerce_tool_args(r.get("raw"))
    if f == "more":
        return live._more(r.get("items", []), int(r.get("shown", 0)), r.get("unit", ""))
    if f == "arg":
        return live._arg(r.get("args", {}), *r.get("keys", []), default=r.get("default", ""))
    return None

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_tools_9c1fca: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
