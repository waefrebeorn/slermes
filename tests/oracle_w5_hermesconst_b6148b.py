#!/usr/bin/env python3
"""Differential oracle for hermes_constants_scratch_stuck.py (w5_hermesconst_b6148b).

OS-bound inputs (errno, euid, lstat fingerprint, os.access) are monkeypatched
so the LIVE functions can be called deterministically against the C port.
"""
import importlib.util
import json
import shlex
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
SRC = Q + "/hermes_constants_scratch_stuck.py"
spec = importlib.util.spec_from_file_location("hcsstuck_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hcsstuck_live"] = live
spec.loader.exec_module(live)

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
    got = r.get("out")

    if func == "is_permission_error":
        exc = OSError(int(r.get("err", 0)), "x") if r.get("is_oserror") else RuntimeError("x")
        py = live.is_permission_error(exc)
        got = bool(got)
    elif func == "accessible":
        lex, acc = bool(r.get("lexists")), bool(r.get("access_ok"))
        old_lex, old_acc = live.os.path.lexists, live.os.access
        live.os.path.lexists = lambda p: lex
        live.os.access = lambda p, m, **kw: acc
        try:
            py = bool(live._accessible("probe"))
        finally:
            live.os.path.lexists, live.os.access = old_lex, old_acc
        got = bool(got)
    elif func == "still_stuck":
        rec = json.loads(r["rec"])
        fp = json.loads(r["fp"])
        acc = json.loads(r["access"])
        uid, now = int(r.get("uid", 0)), float(r.get("now", 0))
        blockers = rec.get("blockers")
        str_acc = ([acc[i] for i, b in enumerate(blockers) if isinstance(b, str)]
                   if isinstance(blockers, list) else [])
        it = iter(str_acc)
        old_g = getattr(live.os, "geteuid", None)
        old_fp, old_a = live._fingerprint, live._accessible
        live.os.geteuid = lambda: uid
        live._fingerprint = lambda p: fp
        live._accessible = lambda b: next(it, False)
        try:
            py = bool(live._still_stuck(Path("root"), "name", rec, now))
        finally:
            live._fingerprint, live._accessible = old_fp, old_a
            if old_g is None:
                del live.os.geteuid
            else:
                live.os.geteuid = old_g
        got = bool(got)
    elif func == "shlex_quote":
        py = shlex.quote(r.get("s", ""))
    else:
        continue

    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w5_hermesconst_b6148b: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
