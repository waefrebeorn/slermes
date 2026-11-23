#!/usr/bin/env python3
"""Differential oracle for upstream Hermes tools/approval_floors.py."""
import fnmatch, importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, Q)
NAME = "w8_floors_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/tools/approval_floors.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

def matching(command, patterns):
    cmd = (command or "").strip()
    if not cmd or live._has_allowlist_shell_operator(cmd):
        return None
    for p in patterns:
        pp = p.strip() if isinstance(p, str) else ""
        if pp and (cmd == pp or (any(ch in pp for ch in "*?[") and fnmatch.fnmatchcase(cmd, pp))):
            return pp
    return None

def compute(r):
    f = r.get("func")
    if f == "user_deny":
        return live._user_deny_block_result(r.get("pattern", ""))
    if f == "sudo":
        return live._sudo_stdin_block_result(r.get("description", ""))
    if f == "hardline":
        return live._hardline_block_result(r.get("description", ""), r.get("command", ""))
    if f == "has_operator":
        return live._has_allowlist_shell_operator(r.get("command", ""))
    if f == "matching":
        return matching(r.get("command", ""), r.get("patterns", []))
    if f == "matches":
        return matching(r.get("command", ""), r.get("patterns", [])) is not None
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE floors: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)