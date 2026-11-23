#!/usr/bin/env python3
"""Differential oracle for cron/scheduler_bookkeeping.py (gate w13_schedulerb_e4b4e1)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

Q = "C:/Users/eman5/hermes-agent"
G = "w13_schedulerb_e4b4e1"
if Q not in sys.path:
    sys.path.insert(0, Q)

spec = importlib.util.spec_from_file_location(G + "_live", Q + "/cron/scheduler_bookkeeping.py")
live = importlib.util.module_from_spec(spec)
sys.modules[G + "_live"] = live
spec.loader.exec_module(live)

_FUNCS = ("classify", "fail_empty")


def live_out(rec):
    func = rec.get("func")
    if func == "classify":
        return live._classify_delivery_outcome(
            delivery_error=rec.get("delivery_error"),
            should_deliver=bool(rec.get("should_deliver")),
            unresolved_origin=bool(rec.get("unresolved_origin")),
            normalized_deliver=rec.get("normalized_deliver"),
            incident_acked=bool(rec.get("incident_acked")),
            success=bool(rec.get("success")),
            delivery_queued=rec.get("delivery_queued"),
            notification_suppressed=bool(rec.get("notification_suppressed")),
        )
    if func == "fail_empty":
        d = types.SimpleNamespace(success=bool(rec.get("success")), error=rec.get("error"))
        live.fail_empty_response(d, rec.get("final_response"))
        return {"success": d.success, "error": d.error}
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in _FUNCS:
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%s func=%s C=%r PY=%r" %
                  (G, rec.get("case"), rec.get("func"), got, py), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (G, cases, mism))
    return 1 if (mism or cases == 0) else 0


if __name__ == "__main__":
    sys.exit(main())
