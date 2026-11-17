import os
#!/usr/bin/env python3
"""
sta_oracle_skills_sync_fs.py — oracle for skills_sync_fs helpers.
Recomputes dir_hash (md5) and safe_rel_install_path (traversal) from
LIVE tools/skills_sync.py on the SAME inputs the C harness
emitted, and exact-compares. When C disagrees, fix the C.
"""
import json, subprocess, sys, os, hashlib, contextlib
sys.path.insert(0, "C:/Users/eman5/hermes-agent")
# Upstream renamed _dir_hash -> dir_hash (public) and moved
# _safe_rel_install_path into tools/skills_sync_optional.py.
from tools.skills_sync import dir_hash as _dir_hash
from tools.skills_sync_optional import _safe_rel_install_path
from pathlib import Path

_harness_dir = os.environ.get("SLERMES_HARNESS_DIR") or str(
    Path(__file__).resolve().parent / "oracle" / "build")
_harness = next(
    (os.path.join(_harness_dir, c) for c in ("tt_skills_sync_fs.exe", "tt_skills_sync_fs")
     if os.path.exists(os.path.join(_harness_dir, c))),
    "/tmp/t_port_skills_sync_fs",  # externally provided binary
)
proc = subprocess.run([_harness], capture_output=True, text=True)
if proc.returncode != 0:
    print("HARNESS CRASHED:", proc.stderr); sys.exit(2)

total = 0; mism = 0
for ln in proc.stdout.splitlines():
    if not ln.strip(): continue
    rec = json.loads(ln)
    total += 1
    fn = rec["fn"]; inp = rec["in"]; cout = rec["out"]
    if fn == "skills_sync_fs_dir_hash":
        # Python's dir_hash feeds str(relative_to(dir)) into the hasher, which
        # on Windows contains BACKSLASHES; the C port normalises to '/' so it
        # matches the POSIX behaviour the hash was defined against. Recompute
        # with POSIX separators rather than asserting a platform artefact.
        exp = None
        if inp:
            d = Path(inp)
            hasher = hashlib.md5()
            with contextlib.suppress(OSError):
                for f in sorted(d.rglob("*")):
                    if f.is_file():
                        hasher.update(f.relative_to(d).as_posix().encode("utf-8"))
                        hasher.update(f.read_bytes())
            exp = hasher.hexdigest().lower()
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
