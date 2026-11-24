#!/usr/bin/env python3
"""Differential oracle for tools/connectors/tool.py (gate w19_tool_6f4666)."""
import ast, json, sys, types, __future__
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = Path("C:/Users/eman5/hermes-agent")
if str(Q) not in sys.path:
    sys.path.insert(0, str(Q))
GATE = "w19_tool_6f4666"
SRC = Q / "tools" / "connectors" / "tool.py"
_ANN = __future__.annotations.compiler_flag

# The pure helpers manage_connections delegates to, taken live.
from tools.connectors.targets import (ALL_ACTIONS, MCP_ACTIONS, HostedTarget,
                                      normalize_targets, validate_action)


def tool_error(message, **extra):
    return json.dumps({"error": str(message)}, ensure_ascii=False)


def run_mcp_operation(mcp_targets, action, **kw):
    return json.dumps({"kind": "mcp", "action": action, "targets": list(mcp_targets)},
                      ensure_ascii=False)


def run_managed_action(action, managed, args, **kw):
    return json.dumps({"kind": "managed", "action": action,
                       "targets": [t.name for t in managed]}, ensure_ascii=False)


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    _mod = types.ModuleType("conn_tool_live")
    _mod.__path__ = []
    sys.modules["conn_tool_live"] = _mod
    g = _mod.__dict__
    g.update({"__name__": "conn_tool_live", "json": json,
              "Any": object, "Callable": object, "Dict": dict, "Optional": object,
              "MANAGE_CATALOG_SCHEMA": {}, "manage_catalog": lambda *a, **k: "",
              "normalize_targets": normalize_targets, "validate_action": validate_action,
              "ALIAS_PATTERN": "", "ALL_ACTIONS": ALL_ACTIONS, "MCP_ACTIONS": MCP_ACTIONS,
              "tool_error": tool_error, "run_mcp_operation": run_mcp_operation,
              "run_managed_action": run_managed_action})
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name == "manage_connections":
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()


def targets_of(items):
    out = []
    for d in items or []:
        out.append(HostedTarget(d.get("name"), d.get("alias"), d.get("to")))
    return out


def run_case(r):
    f = r.get("func")
    if f == "normalize":
        managed, mcp, error = normalize_targets(r.get("raw"))
        return {"managed": [{"name": t.name, "alias": t.alias, "to": t.to} for t in managed],
                "mcp": list(mcp), "error": error}
    if f == "validate":
        return validate_action(r.get("action") or "", targets_of(r.get("managed")),
                               list(r.get("mcp") or []))
    if f == "manage":
        return json.loads(G["manage_connections"](r.get("args") or {}))
    return None


def main():
    cases = mism = 0
    known = {"normalize", "validate", "manage"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        f = r.get("func")
        if f not in known:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
