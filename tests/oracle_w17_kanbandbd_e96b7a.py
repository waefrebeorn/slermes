#!/usr/bin/env python3
"""Differential oracle for w17_kanbandbd_e96b7a / hermes_cli/kanban_db_dispatch.py."""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/kanban_db_dispatch.py"
sys.path.insert(0, QUARRY)

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg

# Late-bound collaborators are stubbed so the module imports without pulling in
# ruamel / sqlite. Only the two kanban exit codes are read by the ported helper.
def _stub(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m

_stub("hermes_cli.quiet_single_query", KANBAN_WORKER_EXIT_TRAILER="KANBAN_WORKER_EXIT")
_stub("hermes_cli._startup_fast", project_root_str=lambda: "/")
_stub("hermes_cli.kanban_db", KANBAN_RATE_LIMIT_EXIT_CODE=75,
      KANBAN_TERMINAL_PROVIDER_EXIT_CODE=78)
for _n in ("kanban_db_connect", "kanban_db_default_assignee", "kanban_db_workspace"):
    _stub("hermes_cli." + _n)

spec = importlib.util.spec_from_file_location("kbd_live_w17", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["kbd_live_w17"] = live
spec.loader.exec_module(live)

def compute(r):
    func = r.get("func")
    if func == "wait_status":
        return live._wait_status_from_returncode(int(r.get("returncode", 0)))
    if func == "exit_code_kind":
        kind, code = live._exit_code_kind(int(r.get("code", 0)))
        return {"kind": kind, "code": code}
    if func == "error_fingerprint":
        return live._error_fingerprint(r.get("error_text", ""))
    if func == "worker_survived":
        return live._worker_survived_termination(r.get("termination") or {})
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
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w17_kanbandbd_e96b7a: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w17_kanbandbd_e96b7a {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w17_kanbandbd_e96b7a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
