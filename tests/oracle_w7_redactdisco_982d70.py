#!/usr/bin/env python3
"""Differential oracle for agent/redact_discord.py (w7_redactdisco_982d70 gate)."""
import importlib.util, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/agent/redact_discord.py"
spec = importlib.util.spec_from_file_location("live_redact_discord_w7", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

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
    try:
        if func == "is_token":
            py = bool(live._is_discord_token(r.get("value", "")))
        elif func == "mfa_left":
            py = bool(live._mfa_left_ok(r.get("value", ""), int(r.get("start", 0))))
        elif func == "redact":
            mask = r.get("mask", "<REDACTED>")
            py = live._redact_discord_tokens(r.get("value", ""), lambda t: mask)
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
print(f"ORACLE w7_redactdisco_982d70: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
