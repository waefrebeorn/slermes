#!/usr/bin/env python3
"""Differential oracle for cron/store_health.py (noc_w6_storehealth_0f6948.h)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/cron/store_health.py"
sys.path.insert(0, QUARRY)

if "cron" not in sys.modules:
    _pkg = types.ModuleType("cron")
    _pkg.__path__ = [QUARRY + "/cron"]
    sys.modules["cron"] = _pkg

spec = importlib.util.spec_from_file_location("store_health_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["store_health_live"] = live
spec.loader.exec_module(live)


def _canon(x):
    if x is None:
        return ("null",)
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", round(float(x), 9))
    if isinstance(x, str):
        return ("str", x)
    if isinstance(x, (list, tuple)):
        return ("list", [_canon(v) for v in x])
    if isinstance(x, dict):
        return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def compute(r):
    f = r["func"]
    if f == "probe_interval":
        return live.PROBE_INTERVAL_SECONDS
    if f == "fix_hint":
        return live.FIX_HINT.format(store=r.get("store") or "")
    if f == "describe_error":
        return live.describe_error(OSError(int(r["errnum"]), r.get("text", "")))
    if f == "unwritable_errno":
        return int(r["errnum"]) in live.UNWRITABLE_ERRNOS
    if f == "outage_covers":
        recs = r.get("records") or []
        degraded = {}
        recovered = {}
        if len(recs) > 0:
            degraded["k"] = types.SimpleNamespace(
                since=recs[0]["since"], recovered_at=recs[0].get("recovered_at"))
        if len(recs) > 1:
            recovered["k"] = types.SimpleNamespace(
                since=recs[1]["since"], recovered_at=recs[1].get("recovered_at"))
        live._degraded = degraded
        live._recovered = recovered
        live._key = lambda cron_dir: "k"
        return live.outage_covers("k", r["due_at"], r["grace"])
    if f == "run_keys":
        pairs = live._run_keys(r.get("jobs") or [])
        return sorted([[a, b] for a, b in pairs],
                      key=lambda p: ("" if p[0] is None else str(p[0]),
                                     "" if p[1] is None else str(p[1])))
    if f == "skipped_runs":
        rec = live.StoreDegraded(store="s", since=0.0, error="e",
                                 skipped=set(r.get("skipped") or []))
        return rec.skipped_runs
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
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE storehealth: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    if _canon(py) != _canon(r.get("out")):
        mism += 1
        print(f"MISMATCH storehealth {r.get('func')} case={r.get('case')} "
              f"py={py!r} c={r.get('out')!r}", file=sys.stderr)
print(f"ORACLE storehealth: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
