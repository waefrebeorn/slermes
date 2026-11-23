#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_snapshot.py (w9_clisnapshot_337c7c).

Live _rejoin_cli runs with injected store/CLI stand-ins; its rebuilt
conversation_history is compared to the C port of that transform.
"""
import importlib.util
import json
import sys
import types
import contextlib

G = "w9_clisnapshot_337c7c"
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
if "hermes_cli" not in sys.modules:
    _hc = types.ModuleType("hermes_cli")
    _hc.__path__ = [Q + "/hermes_cli"]
    sys.modules["hermes_cli"] = _hc

def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m

_mod("hermes_cli.cli_commands_session_tools", _pr=lambda *a, **k: None, _t=lambda k, **kw: k)
class _GWCError(Exception):
    pass
_mod("hermes_cli.gateway_client", GatewayClientError=_GWCError)
_mod("hermes_cli.gateway_profile_lifecycle", ack_confirmed=lambda *a, **k: False,
     ack_failure=lambda *a, **k: "")
_mod("hermes_cli.cli_init_mixin", _ROUTE_FLAGS=frozenset(),
     _gateway_launch=lambda cli: {}, lease_handed_over=lambda cli, had: contextlib.nullcontext())
_mod("hermes_cli.gateway_tools", bind_session=lambda cli, conn: None)

SRC = Q + "/hermes_cli/cli_snapshot.py"
spec = importlib.util.spec_from_file_location("clisnapshot_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["clisnapshot_live"] = live
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
        if func == "rejoin_history":
            snapshot = {"stored_session_id": "s", "messages": r.get("messages")}
            class _Store:
                connection = object()
                def attach(self, sid):
                    return snapshot
            cli = types.SimpleNamespace(session_id="s")
            live._rejoin_cli(cli, _Store())
            py = cli.conversation_history
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
