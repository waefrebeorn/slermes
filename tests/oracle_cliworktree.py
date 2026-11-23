#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_commands_worktree.py (cliworktree gate).

move_cli_workspace's outcome and gateway presence are injected per case; the
real enter_new_worktree runs with those stubs and its observable decision
(return value, cleanup call, printed status count) is compared to the C port.
"""
import builtins
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

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
agent.__path__ = []
_mod("agent.i18n", t=lambda *a, **k: "t")

hermes_cli = _mod("hermes_cli")
hermes_cli.__path__ = []
_mod("hermes_cli.cli_commands_session_tools", _t=lambda *a, **k: "_t")
_mod("hermes_cli.cli_chat_error_copy", refusal_text=lambda e: "refusal")
_mod("hermes_cli.gateway_automation", gateway_connection=lambda cli: GW[0])


class GatewayClientError(Exception):
    pass


_mod("hermes_cli.gateway_client", GatewayClientError=GatewayClientError)

MOVE = ["ok"]
GW = [None]


def move_cli_workspace(cli, wt_info):
    if MOVE[0] == "gateway_error":
        raise GatewayClientError("gateway refused")
    if MOVE[0] == "os_error":
        raise OSError("chdir failed")
    return None


_mod("hermes_cli.gateway_session_store", move_cli_workspace=move_cli_workspace)

cli_mod = _mod("cli")
cli_mod._active_worktree = "previous"
CLEANED = [0]
PRINTS = [0]


def cleanup(wt_info):
    CLEANED[0] += 1


cli_mod._cleanup_worktree = cleanup

SRC = Q + "/hermes_cli/cli_commands_worktree.py"
spec = importlib.util.spec_from_file_location("cliworktree_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["cliworktree_live"] = live
spec.loader.exec_module(live)


def capture_print(*a, **k):
    PRINTS[0] += 1


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "enter":
        continue
    MOVE[0] = r.get("move_outcome", "ok")
    GW[0] = None if r.get("gateway_conn_is_none", True) else SimpleNamespace()
    CLEANED[0] = 0
    PRINTS[0] = 0
    wt = r.get("wt_info", {"path": "/tmp/wt"})
    orig = builtins.print
    builtins.print = capture_print
    try:
        ret = live.enter_new_worktree(SimpleNamespace(), wt)
    except Exception as exc:
        print(f"LIVE ERROR args={r}: {exc!r}", file=sys.stderr)
        builtins.print = orig
        continue
    finally:
        builtins.print = orig
    py = {"ok": bool(ret), "cleanup": CLEANED[0] > 0, "prints": PRINTS[0]}
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH enter args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE cliworktree: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
