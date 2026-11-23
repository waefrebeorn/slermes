#!/usr/bin/env python3
"""Oracle for upstream Hermes hermes_cli/cli_unknown_command.py."""
import difflib, importlib.util, json, os, sys
sys.path.insert(0, "C:/Users/eman5/hermes-agent")  # live module imports agent.i18n
os.environ["HERMES_LANGUAGE"] = "en"   # pin the English catalog used by the C port
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/hermes_cli/cli_unknown_command.py"
spec = importlib.util.spec_from_file_location("cmdunknown_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)

def base(typed):
    return typed.split()[0] if typed.split() else typed

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
    if func == "base":
        py = base(r["typed"])
    elif func == "close":
        matches = difflib.get_close_matches(base(r["typed"]), list(r["known"]), n=1, cutoff=0.6)
        py = matches[0] if matches else None
    elif func == "lines":
        py = list(live.unknown_command_lines(r["typed"], r["known"]))
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE cmdunknown: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
