#!/usr/bin/env python3
"""Differential oracle for upstream Hermes tools/browser_session.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, Q)
NAME = "w8_browser_session_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/tools/browser_session.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

def shares(r):
    si = r.get("session_info") or {}
    features = si.get("features") or {}
    if not features.get("local"):
        return False
    return bool(r.get("display")) or bool(r.get("human_holds"))

def compute(r):
    f = r.get("func")
    if f == "is_local":
        return live.is_local_sidecar_key(r.get("key", ""))
    if f == "owner":
        return live.owner_task_id(r.get("key", ""))
    if f == "session_record":
        return {"session_name": "%s_%s" % (r.get("prefix", ""), r.get("hex10", "")),
                "bb_session_id": None, "cdp_url": r.get("cdp_url"), "features": r.get("features")}
    if f == "publish":
        si = dict(r.get("session_info") or {})
        si.setdefault("session_key", r.get("cache_key", ""))
        si.setdefault("owner_task_id", live.owner_task_id(r.get("cache_key", "")))
        si.setdefault("home_key", r.get("home_key", ""))
        return si
    if f == "shares":
        return shares(r)
    if f == "human_holds_shared":
        return shares(r) and bool(r.get("human_holds"))
    if f == "constants":
        return {"local_suffix": live.LOCAL_SUFFIX,
                "session_inactivity_timeout": live.DEFAULT_SESSION_INACTIVITY_TIMEOUT,
                "w8_browsersess_acacf0_inactivity_timeout": live.BROWSER_SESSION_INACTIVITY_TIMEOUT,
                "orphan_reap_interval": live.BROWSER_ORPHAN_REAP_INTERVAL,
                "max_inactivity_cleanup_failures": live.MAX_INACTIVITY_CLEANUP_FAILURES,
                "stale_endpoint_retries": live._STALE_ENDPOINT_RETRIES}
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
print(f"ORACLE browser_session: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)