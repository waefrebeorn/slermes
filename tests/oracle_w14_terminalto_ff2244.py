#!/usr/bin/env python3
"""Differential oracle for Hermes tools/terminal_tool_result.py (gate w14_terminalto_ff2244).

Stdlib-only live module, no stubs needed. Integer exit codes may arrive as
decimal strings in the fixture (numbers are doubles in the C JSON library) and
are coerced with int() here before calling the live functions.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/tools/terminal_tool_result.py"
spec = importlib.util.spec_from_file_location("w14_terminalto_ff2244_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w14_terminalto_ff2244_live"] = live
spec.loader.exec_module(live)


def as_int(v):
    return int(v) if isinstance(v, str) else v


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
        if func == "signal_exit":
            py = live._interpret_signal_exit(as_int(r.get("exit_code", 0)))
        elif func == "exit_code":
            py = live._interpret_exit_code(r.get("command", ""), as_int(r.get("exit_code", 0)))
        elif func == "cwd_fallback":
            py = live._cwd_fallback_note(r.get("fallback"))
        elif func == "env_recreated":
            py = live._ENV_RECREATED_NOTE
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

print(f"ORACLE w14_terminalto_ff2244: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
