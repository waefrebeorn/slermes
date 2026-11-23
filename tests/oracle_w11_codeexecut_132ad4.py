#!/usr/bin/env python3
"""Differential oracle for tools/code_execution_env.py (w11_codeexecut_132ad4 gate).

Loads the LIVE module with a parent-package bootstrap and stubs for its two
heavy call-time imports (tools.env_passthrough, agent.delegation_context). The
delegated-child transform is a faithful copy of scrub_kanban_env() with its
process-context inputs injected from the fixture (Windows CRT env mutation is
observable, Python os.environ is not). Compares the scrubbed-env dict exactly.
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

MARKER = "HERMES_DELEGATED_CHILD_CONTEXT"
KANBAN_KEYS = ("HERMES_KANBAN_TASK", "HERMES_KANBAN_RUN_ID", "HERMES_KANBAN_CLAIM_LOCK",
               "HERMES_KANBAN_GOAL_MODE", "HERMES_KANBAN_GOAL_MAX_TURNS")
CASE = {}

# parent-package bootstrap + sys.modules registration.
tools = types.ModuleType("tools"); tools.__path__ = []
ep = types.ModuleType("tools.env_passthrough")
ep.is_env_passthrough = lambda k: False
ep.resolve_passthrough_value = lambda name, fallback: (CASE.get("resolved") or {}).get(name)
ep.scoped_passthrough_additions = lambda present: CASE.get("scoped_additions") or {}
sys.modules["tools"] = tools
sys.modules["tools.env_passthrough"] = ep
tools.env_passthrough = ep

agent = types.ModuleType("agent"); agent.__path__ = []
dc = types.ModuleType("agent.delegation_context")
dc.DELEGATED_CHILD_ENV_MARKER = MARKER
dc.KANBAN_ENV_KEYS = KANBAN_KEYS


def scrub_kanban_env(env):
    cleaned = {k: v for k, v in env.items() if k not in KANBAN_KEYS}
    if cleaned.get("HERMES_SESSION_SOURCE") == "kanban":
        cleaned.pop("HERMES_SESSION_SOURCE", None)
    inherited = str(env.get(MARKER) or "")
    cleaned[MARKER] = inherited if inherited and inherited != "1" else CASE.get("fence_root", "1")
    return cleaned


def delegated_child_subprocess_env(env=None):
    if env is None:
        env = {}
    ctx = CASE.get("delegated", False)
    os_task = CASE.get("os_task", "")
    if not (ctx or os_task or (env and (env.get("HERMES_KANBAN_TASK") or env.get(MARKER)))):
        return dict(env)
    return scrub_kanban_env(env)


dc.delegated_child_subprocess_env = delegated_child_subprocess_env
sys.modules["agent"] = agent
sys.modules["agent.delegation_context"] = dc
agent.delegation_context = dc

SRC = Q + "/tools/code_execution_env.py"
spec = importlib.util.spec_from_file_location("codeenv_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["codeenv_live"] = live
spec.loader.exec_module(live)


def drive(r):
    global CASE
    CASE = r
    func = r.get("func")
    if func == "is_secret":
        return live.is_secret_env_name(r.get("name", ""))
    if func == "cache_probe":
        cache = dict(r.get("cache") or {})
        live._cache_probe_result(cache, r.get("key", ""), r.get("value"))
        return cache
    if func == "scrub":
        live._IS_WINDOWS = bool(r.get("is_windows", False))
        pt = list(r.get("passthrough") or [])
        return live._scrub_child_env(
            r.get("source_env") or {},
            is_passthrough=lambda k: k in pt,
            is_windows=bool(r.get("is_windows", False)),
            toolchain=frozenset(r.get("toolchain") or []),
        )
    return "UNKNOWN_FUNC"


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    try:
        py = drive(r)
    except BaseException as exc:
        print(f"LIVE ERROR {r.get('func')}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w11_codeexecut_132ad4: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
