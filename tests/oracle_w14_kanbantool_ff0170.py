#!/usr/bin/env python3
"""Differential oracle for tools/kanban_tools_schemas.py (w14_kanbantool_ff0170)."""
import importlib.util
import json
import sys
import types

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


cli = _mod("hermes_cli")
cli.__path__ = [Q + "/hermes_cli"]
_mod("hermes_cli.kanban_workflow",
     ARCHIVED="archived",
     DEFAULT_WORKFLOW={"todo": {}, "in_progress": {}, "done": {}})

SRC = Q + "/tools/kanban_tools_schemas.py"
spec = importlib.util.spec_from_file_location("tools.kanban_tools_schemas_w14_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
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
        if func == "prop":
            py = live._prop(r.get("type_"), r.get("description"))
        elif func == "board_schema_prop":
            py = live._board_schema_prop()
        elif func == "schema":
            py = live._schema(r.get("name"), r.get("description"),
                              r.get("properties"), r.get("required"))
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

print(f"ORACLE w14_kanbantool_ff0170: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
