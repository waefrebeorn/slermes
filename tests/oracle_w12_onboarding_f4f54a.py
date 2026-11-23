#!/usr/bin/env python3
"""Oracle for hermes_cli/onboarding_due.py stateful cache (live module, full exec)."""
import json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/onboarding_due.py"
ns = {}
exec(compile(open(SRC, encoding="utf-8").read(), SRC, "exec"), ns)

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
    if func == "reset":
        ns["reset_for_tests"]()
        py = None
    elif func == "invalidate":
        ns["invalidate"]()
        py = None
    elif func == "store":
        py = ns["_store"](r.get("value"), int(r.get("generation", "0")),
                          r.get("before", ""), r.get("after", ""))
    elif func == "read":
        seq = iter([r.get("before", ""), r.get("after", "")])
        def stamp():
            return next(seq)
        py = ns["read"](lambda: r.get("value"), stamp=stamp)
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_onboarding_f4f54a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
