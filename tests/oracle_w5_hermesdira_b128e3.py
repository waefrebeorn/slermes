#!/usr/bin/env python3
"""Oracle for upstream Hermes hermes_dir_acl.py (w5_hermesdira_b128e3 gate)."""
import importlib.util, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/hermes_dir_acl.py"
spec = importlib.util.spec_from_file_location("hermesdira_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hermesdira_live"] = live
spec.loader.exec_module(live)

constants = {
    "TOKEN_ELEVATION_TYPE_DEFAULT": live.TOKEN_ELEVATION_TYPE_DEFAULT,
    "FILE_ATTRIBUTE_DIRECTORY": live.FILE_ATTRIBUTE_DIRECTORY,
    "FILE_ATTRIBUTE_REPARSE_POINT": live.FILE_ATTRIBUTE_REPARSE_POINT,
}

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
    if func == "strip":
        py = live._strip_win32_prefix(r.get("path", ""))
    elif func == "remedy":
        py = live.unusable_dir_remedy(r.get("path", ""), windows=bool(r.get("windows", False)))
    elif func == "constants":
        py = constants
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w5_hermesdira_b128e3: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
