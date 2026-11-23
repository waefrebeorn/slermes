#!/usr/bin/env python3
"""Differential oracle for agent/plugin_live_notes.py (gate w9_pluginlive_da5eef).

Drives _append / queue_plugin_notice / pop_plugin_notices with a seeded agent
queue and an identity redactor seam (agent.redact_secret_sources is stubbed),
so the compared outputs are purely the module's ordering / replacement / join
/ append rules.  Every input key is echoed by the harness.
"""
import importlib.util
import json
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

sr = types.ModuleType("session_rules")
sr.__path__ = [QUARRY + "/session_rules"]
sys.modules.setdefault("session_rules", sr)
turns = types.ModuleType("session_rules.turns")
turns.flatten_message_text = lambda c, **k: (c if isinstance(c, str) else "")
sys.modules["session_rules.turns"] = turns

rss = types.ModuleType("agent.redact_secret_sources")
rss.redact_secret_source_values = lambda s: s   # identity seam
sys.modules["agent.redact_secret_sources"] = rss

live = _load("agent.plugin_live_notes", QUARRY + "/agent/plugin_live_notes.py")


def compute(r):
    func = r.get("func")
    if func == "append":
        m = {}
        if "content" in r:
            m["content"] = r["content"]
        live._append(m, r.get("notes", ""))
        return m.get("content")
    if func == "queue":
        a = types.SimpleNamespace()
        a._plugin_live_notes = json.loads(json.dumps(r.get("notes", {})))
        live.queue_plugin_notice(a, r.get("plugin_key", ""), r.get("note", ""))
        return a._plugin_live_notes
    if func == "pop":
        a = types.SimpleNamespace()
        a._plugin_live_notes = dict(r.get("notes", {}))
        return live.pop_plugin_notices(a)
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
print(f"ORACLE w9_pluginlive_da5eef: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
