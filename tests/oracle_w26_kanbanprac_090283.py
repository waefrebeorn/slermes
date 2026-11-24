#!/usr/bin/env python3
"""Differential oracle for hermes_cli/kanban_pr_acceptance.py (gate w26_kanbanprac_090283).

``validate_contract`` is called live (its ValueError is the error seam, captured
as ``{"ok": false, "error": ...}``).  ``_classify`` is called live with the
outcome the caller would have picked -- ``check.get("conclusion")`` for a check
run, ``check["state"]`` for a legacy status -- so the exact-head / pending /
conclusion mapping is upstream's.  No ``gh`` call runs: the check objects are
the already-fetched values.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_kanbanprac_090283"

os.environ["USERPROFILE"] = str(REPO.parent)
os.environ["HOME"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli import kanban_pr_acceptance as live  # noqa: E402

WANTED = {"validate_contract", "classify", "constants"}


def validate(value):
    try:
        return {"ok": True, "value": live.validate_contract(value)}
    except ValueError as exc:
        return {"ok": False, "error": str(exc)}


def classify(req):
    check = req.get("check")
    if not isinstance(check, dict):
        check = {}
    is_run = bool(req.get("is_run"))
    sha = req.get("sha") or ""
    outcome = check.get("conclusion") if is_run else check.get("state")
    return live._classify(check, sha, outcome, is_run)


def expected(r):
    f = r.get("func")
    if f == "validate_contract":
        return validate(r.get("value"))
    if f == "classify":
        return classify(r)
    if f == "constants":
        return {
            "gh_login_keys": list(live._GH_LOGIN_KEYS),
            "gh_github_login_keys": list(live._GH_GITHUB_LOGIN_KEYS),
            "gh_transport_keys": list(live._GH_TRANSPORT_KEYS),
            "repo_pattern": live._REPO.pattern,
            "pr_pattern": live._PR.pattern,
        }
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
