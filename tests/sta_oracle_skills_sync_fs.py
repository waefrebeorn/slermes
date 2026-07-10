#!/usr/bin/env python3
"""
sta_oracle_skills_sync_fs.py — oracle for skills_sync_fs helpers.
Recomputes dir_hash (md5) and safe_rel_install_path (traversal) from
LIVE tools/skills_sync.py on the SAME inputs the C harness
emitted, and exact-compares. When C disagrees, fix the C.
"""
import json, subprocess, sys, os, hashlib
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from tools.skills_sync import _dir_hash, _safe_rel_install_path
from pathlib import Path

proc = subprocess.run(["/tmp/t_port_skills_sync_fs"], capture_output=True, text=True)
if proc.returncode != 0:
    print("HARNESS CRASHED:", proc.stderr); sys.exit(2)

total = 0; mism = 0
for ln in proc.stdout.splitlines():
    if not ln.strip(): continue
    rec = json.loads(ln)
    total += 1
    fn = rec["fn"]; inp = rec["in"]; cout = rec["out"]
    if fn == "skills_sync_fs_dir_hash":
        exp = _dir_hash(Path(inp)).lower() if inp else None
        if (exp or "") != (cout or ""):
            mism += 1; print(f"MISMATCH {fn} in={inp!r}\n  C : {cout!r}\n  PY: {exp!r}")
    elif fn == "safe_rel_ok":
        # 'in' is the python path passed; expect normalized join
        exp = None
        try:
            exp = _safe_rel_install_path(Path(inp), Path("/base"))
        except Exception:
            exp = None
        if (exp or "NULL") != (cout if cout != "NULL" else None):
            mism += 1; print(f"MISMATCH {fn} in={inp!r}\n  C : {cout!r}\n  PY: {exp!r}")
    elif fn in ("safe_rel_traversal", "safe_rel_absolute"):
        # these must raise / return None in Python (rejected)
        exp = None
        try:
            r = _safe_rel_install_path(Path(inp), Path("/base"))
            exp = r  # None if it returned None
        except Exception:
            exp = None
        # C prints "NULL" for rejected paths; treat as None.
        cout_val = None if cout == "NULL" else cout
        if (exp or "NULL") != (cout_val or "NULL"):
            mism += 1; print(f"MISMATCH {fn} in={inp!r}\n  C : {cout!r}\n  PY: {exp!r}")

print(f"RESULT: {total - mism}/{total} match, {mism} mismatch")
sys.exit(1 if mism else 0)
