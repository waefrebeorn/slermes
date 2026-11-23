#!/usr/bin/env python3
"""Differential oracle for w7_turnexplain_e66934 -> agent/turn_explainers.py.

Live module loaded via spec_from_file_location. i18n t() is seamed to identity
(key -> key); hermes_constants / storage_user_copy / persistence_assembly are
stubbed for the session_persistence_failed branch.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_pkg("agent")
_mod("agent.i18n", t=lambda key, **kw: key)
_mod("agent.tool_dispatch_helpers",
     _extract_error_preview=lambda r: "", _extract_file_mutation_targets=lambda n, a: [],
     _extract_landed_file_mutation_paths=lambda n, a, r: [])
_mod("agent.tool_result_classification",
     FILE_MUTATING_TOOL_NAMES=frozenset(), file_mutation_result_landed=lambda n, r: False)
_mod("hermes_constants",
     display_hermes_home=lambda: "home", profile_cli_selector=lambda: "--profile",
     get_default_hermes_root=lambda: Path("root"))
_mod("storage_user_copy", STORAGE_RECOVERY_DOCS_URL="docs")
_mod("persistence_assembly", profile_scope=lambda: "scope", store_location=lambda s: "dbpath")

SRC = Q + "/agent/turn_explainers.py"
spec = importlib.util.spec_from_file_location("turnexplain_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["turnexplain_live"] = live
spec.loader.exec_module(live)


def canonical(x):
    if x is None or isinstance(x, (bool, str, int)):
        return x
    if isinstance(x, float):
        return int(x) if x.is_integer() else x
    if isinstance(x, dict):
        return {k: canonical(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [canonical(v) for v in x]
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
    try:
        if func == "persistence_key":
            py = live._persistence_explanation_key(r.get("cause"))
        elif func == "format_explanation":
            py = live.TurnExplainersMixin._format_turn_completion_explanation(
                r.get("reason", ""), r.get("persistence_cause"), None, r.get("model", ""))
        elif func == "neutralize":
            py = live.TurnExplainersMixin._neutralize_footer_paths(r.get("text", ""))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = canonical(r.get("out"))
    if canonical(py) != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={canonical(py)!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w7_turnexplain_e66934: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
