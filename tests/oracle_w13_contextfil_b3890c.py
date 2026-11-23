#!/usr/bin/env python3
"""Differential oracle for agent/context_file_sources.py pure helpers
(gate w13_contextfil_b3890c). The live module imports prompt_builder and
model_metadata; both are stubbed (estimate_tokens_rough is AST-extracted from
the actual live model_metadata.py) so only the live helpers under test run.
"""
import ast
import importlib.util
import json
import re
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"

# live estimate_tokens_rough via AST extraction
eg = {"re": re}
mm = ast.parse(open(QUARRY + "/agent/model_metadata.py", encoding="utf-8").read())
for node in mm.body:
    take = (isinstance(node, ast.Assign) and any(
        isinstance(t, ast.Name) and t.id in ("CHARS_PER_TOKEN", "_CJK_DENSE_RE")
        for t in node.targets)) or (isinstance(node, ast.FunctionDef) and
                                    node.name == "estimate_tokens_rough")
    if take:
        exec(compile(ast.Module(body=[node], type_ignores=[]), "mm", "exec"), eg)
est_tokens = eg["estimate_tokens_rough"]

agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = []
pb = types.ModuleType("agent.prompt_builder")
SCAN = {"hit": False}
pb._scan_for_threats = lambda content, scope=None: SCAN["hit"]
mmmod = types.ModuleType("agent.model_metadata")
mmmod.estimate_tokens_rough = est_tokens
sys.modules["agent"] = agent_pkg
sys.modules["agent.prompt_builder"] = pb
sys.modules["agent.model_metadata"] = mmmod
agent_pkg.prompt_builder = pb
agent_pkg.model_metadata = mmmod

SRC = QUARRY + "/agent/context_file_sources.py"
spec = importlib.util.spec_from_file_location("cfs_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["cfs_live"] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "estimate":
        return est_tokens(r.get("text", ""))
    if f == "entry":
        return live._entry(r.get("label", ""), r.get("path", ""),
                           r.get("content", ""), r.get("status", ""))
    if f == "loaded_status":
        SCAN["hit"] = bool(r.get("scan_hit", False))
        return live._loaded_status(r.get("content", ""),
                                   int(r.get("rendered_len", "0")),
                                   int(r.get("max_chars", "0")),
                                   bool(r.get("user_authored", False)))
    if f == "render_lines":
        return live.render_context_file_lines(r.get("sources", []))
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
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_contextfil_b3890c: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
