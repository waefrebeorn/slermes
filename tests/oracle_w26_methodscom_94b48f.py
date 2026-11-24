#!/usr/bin/env python3
"""Differential oracle for tui_gateway/methods_complete_helpers.py
(gate w26_methodscom_94b48f).

The live module is imported as a package module; only its pure helpers are
called -- ``_fuzzy_basename_rank`` (the @-mention / Cmd-P ranker) and the
``_walk_repo_files`` directory filter -- plus the cache/exclude budgets.  The
repo listing itself (git ls-files, os.walk, the TTL cache) never runs: that is
the subprocess/filesystem seam.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_methodscom_94b48f"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tui_gateway import methods_complete_helpers as live  # noqa: E402

WANTED = {"rank", "keep_dirnames", "constants"}


def rank(req):
    got = live._fuzzy_basename_rank(req.get("name"), req.get("query"))
    return None if got is None else [got[0], got[1]]


def keep_dirnames(req):
    raw = req.get("dirnames")
    dirs = raw if isinstance(raw, list) else []
    kept = [d for d in dirs if isinstance(d, str)
            and d not in live._FUZZY_FALLBACK_EXCLUDES and not d.startswith(".")]
    return kept


def constants():
    return {
        "cache_ttl_s": live._FUZZY_CACHE_TTL_S,
        "cache_max_files": live._FUZZY_CACHE_MAX_FILES,
        "fallback_excludes": sorted(live._FUZZY_FALLBACK_EXCLUDES),
    }


def expected(r):
    f = r.get("func")
    if f == "rank":
        return rank(r)
    if f == "keep_dirnames":
        return keep_dirnames(r)
    if f == "constants":
        return constants()
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
