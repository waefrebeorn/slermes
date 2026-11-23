#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_dispatch.py (w15_pluginsdis_6f9432)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
if "hermes_cli" not in sys.modules:
    _hc = types.ModuleType("hermes_cli")
    _hc.__path__ = [Q + "/hermes_cli"]
    sys.modules["hermes_cli"] = _hc
_hc = sys.modules["hermes_cli"]
_mw = types.ModuleType("hermes_cli.middleware")
_mw.OBSERVER_SCHEMA_VERSION = 1
sys.modules["hermes_cli.middleware"] = _mw
_hc.middleware = _mw

SRC = Q + "/hermes_cli/plugins_dispatch.py"
spec = importlib.util.spec_from_file_location("pluginsdis_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["pluginsdis_live"] = live
spec.loader.exec_module(live)


class _Section:
    def __init__(self, d):
        self.id = d.get("id", "")
        self.content = d.get("content", "")


def compute(r):
    f = r.get("func")
    if f == "valid_section_id":
        return live.is_valid_system_prompt_section_id(r.get("value"))
    if f == "format_section":
        return live.format_system_prompt_section(r.get("section_id", ""),
                                                 r.get("content", ""))
    if f == "format_sections":
        sections = [_Section(x) for x in (r.get("sections") or [])]
        return live.format_system_prompt_sections(sections)
    if f == "hook_call_identity":
        return live._hook_call_identity(r.get("kwargs") or {}, r.get("hook_name", ""))
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
    expected = compute(r)
    got = r.get("out")
    if expected != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={expected!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w15_pluginsdis_6f9432: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
