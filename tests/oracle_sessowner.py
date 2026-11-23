#!/usr/bin/env python3
"""Oracle for upstream Hermes hermes_cli/sessions_owner.py (sessowner gate)."""
import contextlib, importlib.util, io, json, pathlib, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/hermes_cli/sessions_owner.py"
spec = importlib.util.spec_from_file_location("sessowner_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

# Stand-ins for the inline discovery imports used by live_owner().
holder = {"state": "absent", "endpoint": None}
gateway_runtime = types.ModuleType("hermes_cli.gateway_runtime")
gateway_runtime.discover_gateway_endpoint = lambda home: types.SimpleNamespace(
    state=holder["state"], endpoint=holder["endpoint"])
hc = types.ModuleType("hermes_constants")
hc.get_hermes_home = lambda: pathlib.Path(".")
sys.modules["hermes_cli.gateway_runtime"] = gateway_runtime
sys.modules["hermes_constants"] = hc

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    if func == "msg":
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            rc = live.report_owner_failure(r.get("exc", ""))
        py = buf.getvalue()
        py = py[:-1] if py.endswith("\n") else py
        if rc != 1:
            mism += 1
            print(f"MISMATCH msg rc={rc}", file=sys.stderr)
    elif func == "timeout":
        py = live.PRUNE_RPC_TIMEOUT
    elif func == "owner":
        holder["state"] = r.get("state")
        holder["endpoint"] = r.get("endpoint")
        py = live.live_owner()
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE sessowner: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
