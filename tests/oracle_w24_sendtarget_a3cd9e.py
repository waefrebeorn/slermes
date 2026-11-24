#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/send_targets.py
(gate w24_sendtarget_a3cd9e).

The live module is imported from the Python quarry and its parser factories are
called exactly as a platform plugin calls them -- ``parse_nonempty`` directly and
the two ``regex_*_parser`` closures over the pattern the fixture names.  The
module is pure ``re`` + dataclasses, so there is no seam to stub: nothing here
opens a socket, reads a config or sends anything.

  * ``parse_nonempty`` -> (chat_id, thread) or None;
  * ``regex_stripped_parser`` / ``regex_groups_parser`` -> the same;
  * ``StandaloneSendPolicy`` -> its declared field values;
  * ``NOT_EXPLICIT`` -> a plain ``object()`` sentinel.
"""
import dataclasses
import json
import os
import re
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_sendtarget_a3cd9e"

# Import-time code in the live package resolves the user's home; a bare MSYS
# shell exports HOME but no USERPROFILE, which ``Path.home()`` refuses.
if not os.environ.get("USERPROFILE") and not os.environ.get("HOMEPATH"):
    os.environ["USERPROFILE"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from gateway.platforms import send_targets as live  # noqa: E402

WANTED = {"parse_nonempty", "regex_stripped", "regex_groups", "policy", "constants"}

FIELDS = [f.name for f in dataclasses.fields(live.StandaloneSendPolicy)]


def _pair(result):
    return None if result is None else [result[0], result[1]]


def parse_nonempty(req):
    ref = req.get("ref")
    if not isinstance(ref, str):
        return None          # the C port refuses a non-string ref instead of raising
    return _pair(live.parse_nonempty(ref))


def regex_stripped(req):
    pattern, ref = req.get("pattern"), req.get("ref")
    if not isinstance(pattern, str) or not isinstance(ref, str):
        return None
    return _pair(live.regex_stripped_parser(re.compile(pattern))(ref))


def regex_groups(req):
    pattern, ref = req.get("pattern"), req.get("ref")
    if not isinstance(pattern, str) or not isinstance(ref, str):
        return None
    thread_group = bool(req.get("thread_group"))
    return _pair(live.regex_groups_parser(re.compile(pattern),
                                          thread_group=thread_group)(ref))


def policy(req):
    overrides = {k: req[k] for k in req if k in FIELDS}
    return dataclasses.asdict(live.StandaloneSendPolicy(**overrides))


def constants(_req):
    return {"not_explicit_type": type(live.NOT_EXPLICIT).__name__,
            "fields": {f.name: f.default for f in dataclasses.fields(live.StandaloneSendPolicy)}}


def expected(r):
    f = r.get("func")
    if f == "parse_nonempty":
        return parse_nonempty(r)
    if f == "regex_stripped":
        return regex_stripped(r)
    if f == "regex_groups":
        return regex_groups(r)
    if f == "policy":
        return policy(r)
    if f == "constants":
        return constants(r)
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
