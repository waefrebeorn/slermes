#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_aux_tasks.py (gate w16_pluginsaux_851d61)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w16_pluginsaux_851d61"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

_hc = types.ModuleType("hermes_cli")
_hc.__path__ = [QUARRY + "/hermes_cli"]
sys.modules["hermes_cli"] = _hc
_mps = types.ModuleType("hermes_cli.main_provider_setup")
_mps.__path__ = []
_mps._AUX_TASKS = [
    ("vision", "Vision", "image/screenshot analysis"),
    ("compression", "Compression", "context summarization"),
    ("approval", "Approval", "smart command approval"),
    ("mcp", "MCP", "MCP tool reasoning"),
    ("title_generation", "Title generation", "session titles"),
    ("review", "Review", "/review reviewer subagent"),
    ("memory_query_rewrite", "Memory query rewrite", "memory retrieval queries"),
    ("tts_audio_tags", "TTS audio tags", "Gemini TTS tag insertion"),
    ("voice_chat", "Voice chat", "spoken voice-mode replies"),
    ("triage_specifier", "Triage specifier", "kanban spec fleshing"),
    ("kanban_decomposer", "Kanban decomposer", "task decomposition"),
    ("profile_describer", "Profile describer", "auto profile descriptions"),
    ("curator", "Curator", "skill-usage review pass"),
    ("group_router", "Group router", "who answers in auto-routed Group Chats"),
]
sys.modules["hermes_cli.main_provider_setup"] = _mps
_hc.main_provider_setup = _mps

_spec = importlib.util.spec_from_file_location(
    "hermes_cli.plugins_aux_tasks_live", QUARRY + "/hermes_cli/plugins_aux_tasks.py")
live = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = live
_spec.loader.exec_module(live)


def _compute(r):
    f = r.get("func")
    if f == "build_entry":
        try:
            return live.build_auxiliary_task_entry(
                r.get("me", ""), r.get("owner", ""), r.get("key"),
                display_name=r.get("display_name", ""),
                description=r.get("description", ""),
                defaults=r.get("defaults"),
                inherit_from=r.get("inherit_from"),
                registered=r.get("registered") or {})
        except ValueError as e:
            return {"error": str(e)}
    return None


def _canon(x):
    if isinstance(x, (list, tuple)):
        return [_canon(v) for v in x]
    if isinstance(x, dict):
        return {k: _canon(v) for k, v in x.items()}
    if isinstance(x, float) and x.is_integer():
        return int(x)
    return x


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line or "{" not in line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = _compute(r)
    got = r.get("out")
    if _canon(py) != _canon(got):
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)
print("ORACLE %s: %d cases, %d mismatches" % (G, cases, mism))
sys.exit(1 if mism or not cases else 0)
