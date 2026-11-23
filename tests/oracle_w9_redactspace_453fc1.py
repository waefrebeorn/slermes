#!/usr/bin/env python3
"""Differential oracle for agent/redact_spaced_cfg.py (gate w9_redactspace_453fc1).

Drives the LIVE module's two pure decision predicates.  agent.redact's
_CFG_SECRET_WORD_RE and _looks_like_opaque_credential are seeded with their
real implementations (registered stub module), avoiding the heavy quarry
import chain while keeping the compared semantics identical to production.
"""
import importlib.util
import json
import re
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = [QUARRY + "/agent"]
sys.modules.setdefault("agent", agent_pkg)

red = types.ModuleType("agent.redact")
_SECRET_CFG_NAMES = r"(?:api[ .\-]?key|token|secret|passwd|password|credential|auth)"
red._CFG_SECRET_WORD_RE = re.compile(_SECRET_CFG_NAMES, re.IGNORECASE)


def _looks_like_opaque_credential(value):
    # Verbatim from agent/redact.py (the production helper).
    if value == "***" or value.startswith("\u00abredacted:"):
        return True
    if len(value) >= 16 and re.fullmatch(r"[A-Fa-f0-9]+", value):
        return True
    if len(value) >= 20 and re.fullmatch(r"[A-Za-z0-9_./+=-]+", value):
        return True
    if len(value) < 12:
        return False
    return sum(bool(re.search(p, value)) for p in (r"[a-z]", r"[A-Z]", r"[0-9]")) >= 2


red._looks_like_opaque_credential = _looks_like_opaque_credential
sys.modules["agent.redact"] = red

live = _load("agent.redact_spaced_cfg", QUARRY + "/agent/redact_spaced_cfg.py")


def compute(r):
    func = r.get("func")
    if func == "key_names_secret":
        return live._key_names_secret(r.get("name", ""), bool(r.get("dotted", False)))
    if func == "is_code_expression":
        return live._is_code_expression(r.get("key", ""), r.get("value", ""))
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
print(f"ORACLE w9_redactspace_453fc1: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
