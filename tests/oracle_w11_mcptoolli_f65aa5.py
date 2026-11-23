#!/usr/bin/env python3
"""Differential oracle for tools/mcp_tool_lifecycle.py (w11_mcptoolli_f65aa5 gate).

Loads the LIVE module with a parent-package bootstrap and stubs for
hermes_cli.process_identity, tools.mcp_tool_common and tools.mcp_tool_loop.
psutil cmdlines / PID ledgers are injected as data seams; no process is
signalled. PID/pgid/start numbers cross the boundary as decimal strings.
"""
import importlib.util
import json
import sys
import types
from contextlib import nullcontext
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

CASE = {}

# parent-package bootstrap + sys.modules registration.
hcli = types.ModuleType("hermes_cli"); hcli.__path__ = []
pi = types.ModuleType("hermes_cli.process_identity")
pi.is_owner_death_supervisor = lambda argv: (
    len(argv) > 1 and argv[1] == CASE.get("supervisor_script", "\x00"))
pi.release_process_groups = lambda pgids: None
sys.modules["hermes_cli"] = hcli
sys.modules["hermes_cli.process_identity"] = pi
hcli.process_identity = pi

tools = types.ModuleType("tools"); tools.__path__ = []
sys.modules["tools"] = tools
CORE = SimpleNamespace(_lock=nullcontext(), _server_connect_retry_after={}, _server_connect_failures={})
common = types.ModuleType("tools.mcp_tool_common")
common._core = CORE
sys.modules["tools.mcp_tool_common"] = common
tools.mcp_tool_common = common
loopmod = types.ModuleType("tools.mcp_tool_loop")
loopmod._stop_mcp_loop = lambda only_if_idle: CASE.get("loop_result", False)
sys.modules["tools.mcp_tool_loop"] = loopmod
tools.mcp_tool_loop = loopmod

# psutil double (imported inside functions).
class _NoSuchProcess(Exception):
    pass
class _AccessDenied(Exception):
    pass
psutil = types.ModuleType("psutil")
psutil.NoSuchProcess = _NoSuchProcess
psutil.AccessDenied = _AccessDenied
class Process:
    def __init__(self, pid):
        self.pid = pid
    def cmdline(self):
        argv = (CASE.get("cmdlines") or {}).get(str(self.pid))
        if argv is None:
            raise _NoSuchProcess(str(self.pid))
        return argv
psutil.Process = Process
sys.modules["psutil"] = psutil

SRC = Q + "/tools/mcp_tool_lifecycle.py"
spec = importlib.util.spec_from_file_location("lifecyc_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["lifecyc_live"] = live
spec.loader.exec_module(live)


def as_int_map(d):
    return {int(k): v for k, v in (d or {}).items()}


def strmap(d, stringify_values=True):
    if stringify_values:
        return {str(k): str(v) for k, v in d.items()}
    return {str(k): v for k, v in d.items()}


def drive(r):
    global CASE
    CASE = r
    func = r.get("func")
    if func == "filter_children":
        # psutil is always present as a stub; cmdlines drive the per-pid branch.
        sys.modules["psutil"] = psutil
        pids = set(int(x) for x in (r.get("pids") or []))
        return sorted(str(x) for x in live._filter_mcp_children(pids))
    if func == "take_reapable":
        live._orphan_stdio_pids = set(int(x) for x in (r.get("orphan_pids") or []))
        live._orphan_stdio_pid_servers = as_int_map(r.get("orphan_servers"))
        live._stdio_pids = as_int_map(r.get("stdio_pids"))
        live._stdio_pgids = as_int_map(r.get("stdio_pgids"))
        live._stdio_starttimes = as_int_map(r.get("stdio_starttimes"))
        sn = r.get("server_name")
        pids, pgids, starts = live._take_reapable_pids(bool(r.get("include_active", False)), sn)
        state = {
            "orphan_pids": sorted(str(x) for x in live._orphan_stdio_pids),
            "orphan_servers": strmap(live._orphan_stdio_pid_servers, False),
            "stdio_pids": strmap(live._stdio_pids, False),
            "stdio_pgids": strmap(live._stdio_pgids),
            "stdio_starttimes": strmap(live._stdio_starttimes),
        }
        return {"ret": [strmap(pids, False), strmap(pgids), strmap(starts)], "state": state}
    if func == "clear_cooldowns":
        CORE._server_connect_retry_after = dict(r.get("retry_after") or {})
        CORE._server_connect_failures = dict(r.get("failures") or {})
        live._clear_connect_cooldowns(r.get("keys"))
        return {"retry_after": CORE._server_connect_retry_after,
                "failures": CORE._server_connect_failures}
    if func == "stop_loop_if_idle":
        return live._stop_mcp_loop_if_idle()
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

print(f"ORACLE w11_mcptoolli_f65aa5: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
