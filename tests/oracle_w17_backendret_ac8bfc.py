#!/usr/bin/env python3
"""Differential oracle for hermes_cli/backend_retirement.py (gate w17_backendret_ac8bfc).

RetirementFence's private state is seeded per case from the fixture: no wall
clock, no threads, no random tokens. Only the decision methods are replayed.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/hermes_cli/backend_retirement.py"
spec = importlib.util.spec_from_file_location("w17_backendret_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w17_backendret_live"] = live
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
    try:
        f = live.RetirementFence()
        f._now = lambda t=r.get("now", 0): t
        f._expires_at = r.get("expires", 0)
        f._committed = bool(r.get("committed", False))
        f._active = int(r.get("active", 0))
        f._preparing = bool(r.get("preparing", False))
        f._token = r.get("cur") if r.get("has_token", False) else None
        if func == "acquire":
            ok = f.acquire()
            py = {"ok": ok, "active": f._active}
        elif func == "release":
            f.release()
            py = {"active": f._active}
        elif func == "commit":
            py = f.commit(r.get("token"))
        elif func == "cancel":
            py = f.cancel(r.get("token"))
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

print(f"ORACLE w17_backendret_ac8bfc: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
