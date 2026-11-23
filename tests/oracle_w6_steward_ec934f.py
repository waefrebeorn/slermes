#!/usr/bin/env python3
"""Differential oracle for hermes_cli/steward.py (gate w6_steward_ec934f)."""
import importlib.util
import json
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/hermes_cli/steward.py"
NAME = "w6_steward_live"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

tmp = tempfile.TemporaryDirectory()
root = Path(tmp.name)
git = root / ".git"
stamp = root / "install-stamp.json"

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func, got = r.get("func"), r.get("out")
    if r.get("has_git"):
        git.write_text("")
    else:
        try: git.unlink()
        except OSError: pass
    if r.get("content") is not None:
        stamp.write_text(r["content"], encoding="utf-8")
    else:
        try: stamp.unlink()
        except OSError: pass

    if func == "steward_update_message":
        want = live.steward_update_message(r.get("steward", ""))
    elif func == "steward_uninstall_message":
        want = live.steward_uninstall_message(r.get("steward", ""), r.get("platform"))
    elif func == "read_install_stamp":
        want = live.read_install_stamp(root)
    elif func == "sealed_steward":
        want = live.sealed_steward(root)
    elif func == "is_bundled_payload":
        want = live.is_bundled_payload(root)
    elif func == "classify_install":
        want = list(live.classify_install(root))
    else:
        continue
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH {func} in={r} py={want!r} c={got!r}", file=sys.stderr)
tmp.cleanup()
print(f"ORACLE w6_steward_ec934f: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
