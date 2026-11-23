#!/usr/bin/env python3
"""Differential oracle for tools/mcp_elicitation_form.py (w14_mcpelicita_0beae8)."""
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
tools.__path__ = [Q + "/tools"]
_mod("tools.clarify_tool", MAX_CHOICES=5, MAX_QUESTIONS=8)

SRC = Q + "/tools/mcp_elicitation_form.py"
spec = importlib.util.spec_from_file_location("tools.mcp_elicitation_form_w14_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def make_field(spec, required=False):
    kind, options = live._field_kind(spec)
    return live.FormField(name="f", kind=kind, label="f", description="",
                          required=required, default=spec.get("default"),
                          options=options, spec=spec)


def norm(x):
    if isinstance(x, tuple):
        return [norm(v) for v in x]
    if isinstance(x, list):
        return [norm(v) for v in x]
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    return x


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
    s = r.get("spec")
    try:
        if func == "options":
            py = live._options(s)
        elif func == "field_kind":
            py = live._field_kind(s)
        elif func == "match_option":
            py = live._match_option(make_field(s), r.get("text", ""))
        elif func == "coerce_boolean":
            py = live._coerce_boolean(None, r.get("text", ""))
        elif func == "format_error":
            py = live._format_error(r.get("fmt", ""), r.get("text", ""))
        elif func == "coerce_string":
            py = live._coerce_string(make_field(s), r.get("text", ""))
        elif func == "coerce_number":
            py = live._coerce_number(make_field(s), r.get("text", ""))
        elif func == "coerce_multi":
            py = live._coerce_multi(make_field(s), r.get("raw"))
        elif func == "coerce_answer":
            py = live.coerce_answer(make_field(s, required=bool(r.get("required", False))),
                                    r.get("raw"))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if norm(py) != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w14_mcpelicita_0beae8: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
