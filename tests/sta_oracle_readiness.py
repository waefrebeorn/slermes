#!/usr/bin/env python3
"""Oracle for gateway/readiness.py pure probes.

The probes do IO (disk_usage, sqlite, config read) that the port takes as
parameters, so the expected values here are the branch logic transcribed
verbatim from the source — the exact status strings and dict keys are copied.
"""
import json
import sys

DISK_DEGRADED = 90.0
CONNECTED = {"connected", "running", "ok"}


def probe_state_db(exists, corrupt, error):
    if not exists:
        return {"status": "ok", "detail": "not initialized"}
    if corrupt:
        return {"status": "degraded", "detail": "corrupt"}
    if error:
        return {"status": "degraded", "detail": error}
    return {"status": "ok"}


def probe_config(exists, backend_unavail, exc, raw_mapping):
    if not exists:
        return {"status": "ok", "detail": "using defaults"}
    if backend_unavail:
        return {"status": "degraded", "detail": "config backend unavailable"}
    if exc:
        return {"status": "degraded", "detail": f"invalid config ({exc})"}
    if not raw_mapping:
        return {"status": "degraded", "detail": "top level is not a mapping"}
    return {"status": "ok"}


def probe_disk(used, free, error):
    if error:
        return {"status": "degraded", "detail": error}
    st = "degraded" if used >= DISK_DEGRADED else "ok"
    return {"status": st, "used_percent": used, "free_bytes": free}


def probe_gateway(state, platforms):
    state = state or "unknown"
    st = "ok" if state in {"running", "draining"} else "degraded"
    connected = sum(
        1 for v in (platforms or {}).values()
        if isinstance(v, dict)
        and str(v.get("state") or v.get("status") or "").lower() in CONNECTED)
    return {"status": st, "state": state,
            "connected_platforms": connected, "platforms": len(platforms or {})}


def probe_session_store(db_detail, db_status, store):
    if db_detail == "corrupt":
        return {"status": "unavailable", "detail": "corrupt"}
    if store in ("ok", "unavailable", "retrying"):
        return {"status": store}
    return {"status": "ok" if db_status == "ok" else "unavailable"}


def overall(arr):
    return "ok" if (
        isinstance(arr, list) and arr
        and all(isinstance(c, dict) and c.get("status") == "ok" for c in arr)
    ) else ("ok" if arr == [] else "degraded")


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE readiness: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "sdb":
            want = probe_state_db(bool(c["exists"]), bool(c["corrupt"]), c["error"])
        elif fam == "cfg":
            want = probe_config(bool(c["exists"]), bool(c["backend"]), c["exc"],
                                bool(c["mapping"]))
        elif fam == "dsk":
            want = probe_disk(c["used"], c["free"], c["error"])
        elif fam == "gw":
            pj = c["platforms"]
            want = probe_gateway(c["state"],
                                 json.loads(pj) if pj else None)
        elif fam == "ss":
            want = probe_session_store(c["db_detail"], c["db_status"], c["store"])
        elif fam == "ov":
            want = overall(c["arr"])  # already decoded
        else:
            want = None
        got = c["out"]  # already decoded by the outer json.loads
        if got != want:
            mism += 1
            print(f"MISMATCH {fam} {c['case']}: C={got!r} PY={want!r}")
    print(f"ORACLE readiness: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
