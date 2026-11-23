#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_shutdown.py (w9_clishutdown_5f546e)."""
import importlib.util
import json
import sys
import types

G = "w9_clishutdown_5f546e"
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
if "hermes_cli" not in sys.modules:
    _hc = types.ModuleType("hermes_cli")
    _hc.__path__ = [Q + "/hermes_cli"]
    sys.modules["hermes_cli"] = _hc

SRC = Q + "/hermes_cli/cli_shutdown.py"
spec = importlib.util.spec_from_file_location("clishutdown_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["clishutdown_live"] = live
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
        if func == "float_env":
            default = r.get("default", 30.0)
            env = r.get("env_value", None)
            live.os = types.SimpleNamespace(getenv=lambda k, d=None: (default if env is None else env))
            py = live._float_env("HERMES_EXIT_WATCHDOG_S", default)
        elif func == "should_emit":
            live._gateway_owns_session_end = lambda agent: bool(r.get("gateway_owns"))
            live._cli = lambda: types.SimpleNamespace(
                _active_agent_ref=object(),
                _handed_off_session_ids=set(r.get("handed_off") or []),
                _single_query_finalize_attempted_session_ids=set(r.get("attempted") or []))
            py = live._should_emit_cleanup_session_finalize(r.get("session_id"))
        elif func == "oneshot_session":
            agent = types.SimpleNamespace(session_id=r.get("agent_session"))
            cli = types.SimpleNamespace(agent=agent, session_id=r.get("cli_session"))
            _a, py = live._oneshot_agent_and_session(cli)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
