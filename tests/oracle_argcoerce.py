#!/usr/bin/env python3
"""Differential oracle for upstream Hermes tools/arg_coercion.py (argcoerce gate).

The two module-level imports (tools.registry, tools.schema_sanitizer) are stubbed
so the live module loads; resolve_local_schema_ref is identity (fixtures use no
$ref), matching the C port.
"""
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


tools = _mod("tools")
tools.__path__ = []
_mod("tools.registry", registry=types.SimpleNamespace(get_schema=lambda n: {}))
_mod("tools.schema_sanitizer",
     resolve_local_schema_ref=lambda s, root: s,
     unrename_tool_args=lambda params, args: args)

SRC = Q + "/tools/arg_coercion.py"
spec = importlib.util.spec_from_file_location("argcoerce_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["argcoerce_live"] = live
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
        if func == "number":
            py = live._coerce_number(r.get("value", ""), bool(r.get("integer_only", False)))
        elif func == "boolean":
            py = live._coerce_boolean(r.get("value", ""))
        elif func == "allows_null":
            py = live._schema_allows_null(r.get("schema"))
        elif func == "accepts_kind":
            py = live._schema_accepts_kind(r.get("schema"), r.get("kind", "array"), r.get("root"))
        elif func == "coerce_value":
            py = live._coerce_value(r.get("value", ""), r.get("expected_type"), schema=r.get("schema"))
        elif func == "normalize":
            py = live._normalize_json_strings_for_schema(r.get("value"), r.get("schema"), r.get("root"))
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

print(f"ORACLE argcoerce: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
