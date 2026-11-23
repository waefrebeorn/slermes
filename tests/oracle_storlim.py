#!/usr/bin/env python3
"""Oracle for upstream Hermes storage_limits.py.

The live _configured_transcript_limit lazily imports hermes_cli.config for an
external config.yaml read; we inject a controlled fake so the unit under test
is the pure config->limit semantics. stdout/stderr forced to UTF-8.
"""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

cfg = types.ModuleType("hermes_cli.config")
holder = {"sessions": None}
cfg.load_config_readonly = lambda: {"sessions": holder["sessions"]}
sys.modules["hermes_cli"] = types.ModuleType("hermes_cli")
sys.modules["hermes_cli.config"] = cfg

SRC = "C:/Users/eman5/hermes-agent/storage_limits.py"
spec = importlib.util.spec_from_file_location("storlim_live", SRC)
live = importlib.util.module_from_spec(spec)
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
    holder["sessions"] = r.get("sessions")
    if func == "resume":
        val = live.resolved_max_resume_messages()
    elif func == "export":
        val = live.resolved_max_export_messages()
    elif func == "limit":
        val = live._configured_transcript_limit(r["key"], int(r["fallback"]))
    else:
        continue
    py = str(val)
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE storlim: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
