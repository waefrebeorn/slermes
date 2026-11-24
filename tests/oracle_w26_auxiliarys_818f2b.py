#!/usr/bin/env python3
"""Differential oracle for agent/auxiliary_structured_output.py (gate w26_auxiliarys_818f2b).

Two seams are supplied as fixed inputs:

  * the provider-profile registry -- ``unsupported`` is what
    ``_profile_unsupported_formats(provider, base_url)`` would answer, injected by
    patching that function, so the drop decision is the module's own;
  * the process memo -- ``rejected`` seeds ``_REJECTED_ROUTES`` before the call.

Everything else (route keys, format types, capability markers, the retry kwargs)
is the live module's code.
"""
import json
import os
import sys
from pathlib import Path
from unittest import mock

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_auxiliarys_818f2b"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))
os.environ.setdefault("USERPROFILE", str(QUARRY.parent))
os.environ.setdefault("HOME", str(QUARRY.parent))

from agent import auxiliary_structured_output as live  # noqa: E402

WANTED = {"route_key", "format_type", "capability", "without_format", "strip_format",
          "remember", "constants"}


def route_key(req):
    return live._route_key(req.get("provider"), req.get("base_url"))


def format_type(req):
    kwargs = req.get("kwargs")
    return live._response_format_type(kwargs if isinstance(kwargs, dict) else {})


def capability(req):
    return bool(live.is_capability_rejection(req.get("error")))


def without_format(req):
    extra_body = req.get("extra_body")
    extra_body = extra_body if isinstance(extra_body, dict) else {}
    unsupported = tuple(req.get("unsupported") or ())
    rejected = {tuple(row) for row in (req.get("rejected") or [])}
    with mock.patch.object(live, "_profile_unsupported_formats", lambda provider, base_url: unsupported):
        live._REJECTED_ROUTES.clear()
        live._REJECTED_ROUTES.update(rejected)
        try:
            return live.without_unsupported_response_format(
                extra_body, req.get("provider"), req.get("base_url"), req.get("model"), req.get("task"),
            )
        finally:
            live._REJECTED_ROUTES.clear()


def strip_format(req):
    kwargs = req.get("kwargs")
    return live._without_structured_output_format(kwargs if isinstance(kwargs, dict) else {})


def remember(req):
    kwargs = req.get("kwargs")
    kwargs = kwargs if isinstance(kwargs, dict) else {}
    live._REJECTED_ROUTES.clear()
    try:
        live.remember_structured_output_rejection(
            req.get("provider"), req.get("base_url"), kwargs, req.get("error"),
        )
        entries = sorted(live._REJECTED_ROUTES)
    finally:
        live._REJECTED_ROUTES.clear()
    return list(entries[0]) if entries else None


def constants():
    return {"capability_rejection_markers": list(live._CAPABILITY_REJECTION_MARKERS)}


def expected(r):
    f = r.get("func")
    if f == "route_key":
        return route_key(r)
    if f == "format_type":
        return format_type(r)
    if f == "capability":
        return capability(r)
    if f == "without_format":
        return without_format(r)
    if f == "strip_format":
        return strip_format(r)
    if f == "remember":
        return remember(r)
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
