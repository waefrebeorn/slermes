#!/usr/bin/env python3
"""Differential oracle for tools/skill_manager_guards.py (w14_skillmanag_328d03)."""
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


agent = _mod("agent")
agent.__path__ = [Q + "/agent"]
skill_utils = _mod("agent.skill_utils", ESSENTIAL_SKILLS=set())

SRC = Q + "/tools/skill_manager_guards.py"
spec = importlib.util.spec_from_file_location("tools.skill_manager_guards_w14_live", SRC)
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
    skill_utils.ESSENTIAL_SKILLS = set(r.get("essential") or [])
    live._is_pinned = lambda name, what: r.get("pinned")
    live._is_background_review = lambda: bool(r.get("is_bg", False))
    live._background_review_has_read = lambda path: bool(r.get("has_read", False))
    try:
        if func == "refusal":
            py = live._refusal(r.get("message", ""), **(r.get("extra") or {}))
        elif func == "pinned_guard":
            py = live._pinned_guard(r.get("name", ""))
        elif func == "read_before_write_guard":
            py = live._background_review_read_before_write_guard(
                r.get("name", ""), r.get("target", ""),
                r.get("action", ""), r.get("file_label", ""))
        elif func == "curator_consolidation_delete_guard":
            py = live._curator_consolidation_delete_guard(
                r.get("name", ""), r.get("absorbed_into"))
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

print(f"ORACLE w14_skillmanag_328d03: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
