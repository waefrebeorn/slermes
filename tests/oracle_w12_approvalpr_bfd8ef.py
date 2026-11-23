#!/usr/bin/env python3
"""Differential oracle for tools/approval_provenance.py (gate w12_approvalpr_bfd8ef)."""
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
G = "w12_approvalpr_bfd8ef"
sys.path.insert(0, QUARRY)

SRC = QUARRY + "/tools/approval_provenance.py"
_spec = importlib.util.spec_from_file_location("approval_provenance_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["approval_provenance_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "preapproved":
        findings = [tuple(pair) for pair in rec["findings"]]
        scopes = rec["scopes"]
        return live.preapproved_result(findings, lambda k: scopes[k])
    if func == "allowlisted":
        return live.allowlisted_result(rec.get("rule", ""), rec.get("description", ""))
    if func == "note":
        return live.preapproval_note(rec["entries"])
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
        if rec.get("func") not in ("preapproved", "allowlisted", "note"):
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
