#!/usr/bin/env python3
"""Differential oracle for hermes_cli/fallback_check.py (gate w12_fallbackch_b8adfb)."""
import importlib.util
import json
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w12_fallbackch_b8adfb"
sys.path.insert(0, QUARRY)

SRC = QUARRY + "/hermes_cli/fallback_check.py"
_spec = importlib.util.spec_from_file_location("fallback_check_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["fallback_check_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "canonical":
        return live._canonical_model_id(rec.get("model", ""))
    if func == "answered":
        return live.answered_by_requested_model(rec.get("requested", ""), rec.get("returned", ""))
    if func == "status":
        e = live.EntryCheck(
            provider="p", model="m",
            configured=bool(rec.get("configured", False)),
            auth_ready=bool(rec.get("auth_ready", False)),
            inference_ready=rec.get("inference", None),
            error_class=rec.get("error_class", ""),
        )
        return {"ready": e.ready, "status": e.status}
    if func == "secrets":
        return list(live._secret_values(*(rec.get("candidates") or [])))
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
        if rec.get("func") not in ("canonical", "answered", "status", "secrets"):
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} func={rec.get('func')} "
                  f"C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
