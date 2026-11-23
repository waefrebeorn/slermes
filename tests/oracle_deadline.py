#!/usr/bin/env python3
"""Oracle for agent/tool_executor_deadline.py.

The module re-imports `registry` inside declared_deadline_floor, so we inject
a deterministic tools.registry module per case whose get_deadline_floor returns
the fixture's `reg_floor` for the named tool.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

ROOT = "C:/Users/eman5/hermes-agent"
SRC = ROOT + "/agent/tool_executor_deadline.py"
spec = importlib.util.spec_from_file_location("deadline_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)


def as_opt(v):
    return None if v is None else float(v)


def install_registry(mapping):
    class FakeReg:
        def get_deadline_floor(self, name, args):
            return mapping.get(name)

    mod = types.ModuleType("tools.registry")
    mod.registry = FakeReg()
    sys.modules["tools.registry"] = mod


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
    if func == "declared":
        name = r.get("name", "")
        install_registry({name: as_opt(r.get("reg_floor"))})
        py = live.declared_deadline_floor(name, {})
    elif func == "with_floor":
        py = live.deadline_with_floor(as_opt(r.get("generic")),
                                      as_opt(r.get("floor")))
    elif func == "batch":
        mapping = {}
        calls = []
        for c in r.get("calls", []):
            name = c.get("name", "")
            mapping[name] = as_opt(c.get("reg_floor"))
            calls.append(SimpleNamespace(
                parse_error=("x" if c.get("parse_error") else None),
                name=name,
                args={},
            ))
        install_registry(mapping)
        py = live.batch_deadline(as_opt(r.get("generic")), calls)
    else:
        continue

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE deadline: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
