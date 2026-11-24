#!/usr/bin/env python3
"""Differential oracle for tools/escape_drift_autocorrect.py (gate w26_escapedrif_a472f8).

The live module has no I/O at all: every entry point is a pure function of
(old_string, new_string, file content).  The file itself is the seam and arrives
as a fixture string, so what is compared is exactly the decision the patch tool
would take -- including the ``AmbiguousEscapeDriftError`` refusal and the
``TypeError`` a non-string argument raises.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_escapedrif_a472f8"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tools import escape_drift_autocorrect as live  # noqa: E402

WANTED = {
    "halve", "runs_even", "strip_quotes", "quotes_all_spurious",
    "backslash_correct", "quote_correct", "escape_drift", "notes",
}

ERR = live.AmbiguousEscapeDriftError


def _pair(fn, req):
    old, new, content = req.get("old"), req.get("new"), req.get("content")
    try:
        co, cn = fn(old, new, content)
    except ERR:
        return {"old": None, "new": None, "error": "AmbiguousEscapeDriftError"}
    return {"old": co, "new": cn, "error": None}


def expected(r):
    f = r.get("func")
    if f == "halve":
        return live._halve_backslash_runs(r.get("s"))
    if f == "runs_even":
        return live._all_backslash_runs_even(r.get("s"))
    if f == "strip_quotes":
        return live._strip_spurious_quote_escapes(r.get("s"))
    if f == "quotes_all_spurious":
        strings = r.get("strings")
        if not isinstance(strings, list) or not all(isinstance(s, str) for s in strings):
            raise TypeError("strings must be a list of str")
        return live._quote_escapes_all_spurious(*strings)
    if f == "backslash_correct":
        return _pair(live.maybe_correct_backslash_doubling, r)
    if f == "quote_correct":
        return _pair(live.maybe_correct_quote_escaping, r)
    if f == "escape_drift":
        old, new, content = r.get("old"), r.get("new"), r.get("content")
        try:
            co, cn, note = live.maybe_correct_escape_drift(old, new, content)
        except ERR:
            return {"old": None, "new": None, "error": "AmbiguousEscapeDriftError",
                    "note": None}
        return {"old": co, "new": cn, "error": None, "note": note}
    if f == "notes":
        return {
            "quote": ("escape-drift auto-corrected: stripped a spurious backslash before a "
                      "quote/apostrophe in old_string/new_string (a JSON-double-escaping wire "
                      "artifact) before matching."),
            "backslash": ("escape-drift auto-corrected: halved doubled backslash runs in "
                          "old_string before matching; new_string has no backslashes and was "
                          "left unchanged."),
            "ambiguous": "AmbiguousEscapeDriftError",
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
        except AttributeError:
            py = {"error": "AttributeError"}
        except TypeError:
            py = {"error": "TypeError"}
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
