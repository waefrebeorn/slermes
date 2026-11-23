#!/usr/bin/env python3
"""Oracle for upstream Hermes storage_user_copy.py (w5_storageuser_ed737a gate).

describe_storage_failure's three external facts are injected by patching the
live module's classify/is-disk-full imports and the hermes_constants selector,
matching the C seam that takes those values as explicit arguments.
"""
import importlib.util, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/storage_user_copy.py"
spec = importlib.util.spec_from_file_location("storageuser_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["storageuser_live"] = live
spec.loader.exec_module(live)

import hermes_constants as hc

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
    if func == "docs":
        py = live.STORAGE_RECOVERY_DOCS_URL
    elif func == "details":
        py = live.storage_failure_details(r.get("text"), limit=r.get("limit", 200))
    elif func == "describe":
        cause = r.get("cause", "")
        disk_full = bool(r.get("disk_full", False))
        profile_arg = r.get("profile_arg", "")
        live.classify_persistence_error = lambda x, c=cause: c
        live.is_disk_full_error = lambda x, d=disk_full: d
        hc.profile_cli_selector = lambda: profile_arg
        f = live.describe_storage_failure("x")
        py = {"cause": f.cause, "code": f.code, "gloss": f.gloss, "action": f.action}
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w5_storageuser_ed737a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
