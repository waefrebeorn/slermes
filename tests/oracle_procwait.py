#!/usr/bin/env python3
"""Oracle for upstream Hermes tools/process_registry_wait.py (gate procwait)."""
import importlib.util, json, os, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/tools/process_registry_wait.py"
spec = importlib.util.spec_from_file_location("procwait_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")

    # Per-case TERMINAL_TIMEOUT, mirrored by the C harness via setenv/unsetenv.
    os.environ.pop("TERMINAL_TIMEOUT", None)
    if r.get("terminal_timeout") is not None:
        os.environ["TERMINAL_TIMEOUT"] = str(r["terminal_timeout"])

    if func == "max_wait_timeout":
        py = live.max_wait_timeout()
    elif func == "process_deadline_floor":
        py = live.process_deadline_floor(r.get("args", {}))
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} {r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE procwait: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
