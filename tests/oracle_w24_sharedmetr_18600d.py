#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/shared_metrics.py
(gate w24_sharedmetr_18600d).

The router module itself is imported live for its request model
(``ConsentAnswer``); the offer state it serves is the live
``hermes_cli.observability.shared_metrics_consent`` module (``consent_state``,
``set_answer`` and the offer contract) -- the same two functions this router
calls inside ``_read_consent``/``_write``.  The HTTP layer, the thread hop and
the config file never run: the case supplies the raw config mapping and the
managed flag, and what is compared is the answer those values produce.
"""
import copy
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_sharedmetr_18600d"

# A bash login shell drops USERPROFILE, and the live modules resolve the Hermes
# home through pathlib.Path.home() at import time.  The repository's parent is
# that home; give the import the variable it needs instead of a temp dir.
os.environ.setdefault("USERPROFILE", str(REPO.parent))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from hermes_cli.web_routers import shared_metrics as live  # noqa: E402
from hermes_cli.observability import shared_metrics_consent as consent  # noqa: E402

WANTED = {"answer", "consent_view", "set_answer", "constants"}


def answer(req):
    kwargs = {}
    if "enabled" in req:
        kwargs["enabled"] = req["enabled"]
    if "send" in req:
        kwargs["send"] = req["send"]
    try:
        return dict(live.ConsentAnswer(**kwargs).model_dump())
    except Exception:  # noqa: BLE001 - pydantic ValidationError
        return {"error": "ValidationError"}


def consent_view(req):
    return {**consent.consent_state(req.get("config")), "managed": bool(req.get("managed"))}


def set_answer(req):
    target = copy.deepcopy(req.get("target"))
    if target is None:
        target = {}
    consent.set_answer(target, bool(req.get("enabled")), bool(req.get("send")))
    return target


def constants():
    return {
        "offer_version": consent.OFFER_VERSION,
        "choices": [list(choice) for choice in consent.OFFER_CHOICES],
        "no_thanks": consent._NO_THANKS,
        "docs_url": consent.DOCS_URL,
    }


def expected(r):
    f = r.get("func")
    if f == "answer":
        return answer(r)
    if f == "consent_view":
        return consent_view(r)
    if f == "set_answer":
        return set_answer(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def eq(a, b):
    """Python equality: absent key == null == None, True == 1, 1 == 1.0."""
    if isinstance(a, bool) or isinstance(b, bool):
        return isinstance(a, (bool, int, float)) and isinstance(b, (bool, int, float)) and a == b
    if isinstance(a, dict) and isinstance(b, dict):
        return set(a) == set(b) and all(eq(a[k], b[k]) for k in a)
    if isinstance(a, list) and isinstance(b, list):
        return len(a) == len(b) and all(eq(x, y) for x, y in zip(a, b))
    if a is None or b is None:
        return a is b
    return a == b


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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if not eq(py, got):
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
