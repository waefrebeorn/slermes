#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_single_query.py (gate w15_clisingle_3677f3).

Stubs the three module-load imports (agent.i18n, agent.interrupt_compat,
hermes_cli.turn_exit). The ported helpers are pure: no process state is touched.
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
    parent, _, child = name.rpartition(".")
    if parent and parent in sys.modules:
        setattr(sys.modules[parent], child, m)
    return m


agent = _mod("agent")
agent.__path__ = [Q + "/agent"]
hcli = _mod("hermes_cli")
hcli.__path__ = [Q + "/hermes_cli"]
_mod("agent.i18n", t=lambda key, **kw: key)
_mod("agent.interrupt_compat", request_hard_interrupt=lambda *a, **k: None)
_mod("hermes_cli.turn_exit",
     TRANSIENT_PROVIDER_REASONS=frozenset(),
     TERMINAL_PROVIDER_REASONS=frozenset())

SRC = Q + "/hermes_cli/cli_single_query.py"
spec = importlib.util.spec_from_file_location("hermes_cli.cli_single_query", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hermes_cli.cli_single_query"] = live
spec.loader.exec_module(live)


class _Agent:
    pass


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
        if func == "int_or":
            py = live._int_or(r.get("value"), r.get("default"))
        elif func == "sync_session":
            cli = _Agent()
            cli.session_id = r["cli"].get("session_id")
            cli.agent = None
            if isinstance(r["cli"].get("agent"), dict):
                cli.agent = _Agent()
                cli.agent.session_id = r["cli"]["agent"].get("session_id")
            live._sync_cli_session_id_from_agent(cli)
            py = {"session_id": cli.session_id}
        elif func == "configure_quiet":
            ag = _Agent()
            for k, v in (r.get("agent") or {}).items():
                setattr(ag, k, v)
            live._configure_quiet_agent(ag)
            py = dict(r.get("agent") or {})
            for k in ("quiet_mode", "suppress_status_output", "stream_delta_callback",
                      "tool_gen_callback", "reasoning_callback", "tool_progress_callback",
                      "tool_start_callback", "tool_complete_callback", "tool_progress_mode"):
                py[k] = getattr(ag, k)
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

print(f"ORACLE w15_clisingle_3677f3: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
