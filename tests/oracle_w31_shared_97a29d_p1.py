#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/_shared.py ingress primitives
(gate w31_shared_97a29d_p1).

The live module is imported straight out of the quarry and its pure helpers are
called with the fixture's values.  Every I/O seam is injected, never touched:

  * ``secrets_match`` needs nothing: both sides are values;
  * ``timestamp_fresh`` reads the clock through the module's ``time`` binding,
    so the oracle swaps ``live.time`` for a one-method stand-in pinned to the
    fixture's ``now`` -- the real function body still runs;
  * ``decode_json_list_literal`` / ``coerce_port`` need nothing;
  * ``request_never_left`` walks ``type(exc).__mro__`` and ``__cause__`` /
    ``__context__``, so the oracle builds plain exception classes carrying the
    fixture's class names and strings them into a cause chain -- again the real
    body walks them.

What is compared is therefore exactly what those values decide.  ``str.isspace``
and ``int(str)`` are CPython's, which the port spells out for the ASCII subset
the gateway feeds them (headers, env values, ports).
"""
import json
import os
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_shared_97a29d_p1"

# The quarry resolves its data dir from the OS home at import time; a bare
# ``bash -lc`` environment has none, so pin it to the repo's own parent.
if not os.environ.get("HOME") and not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)
    os.environ["HOME"] = str(QUARRY.parent)
os.environ.setdefault("LOCALAPPDATA", str(QUARRY.parent / "AppData" / "Local"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import gateway.platforms._shared as live  # noqa: E402

WANTED = {"secrets_match", "timestamp_fresh", "decode_json_list_literal",
          "coerce_port", "bridged_env_marker", "request_never_left"}


def secrets_match(req):
    return bool(live.secrets_match(req.get("provided"), req.get("expected")))


def timestamp_fresh(req):
    """``abs(time.time() - int(raw)) <= window`` with the clock handed in."""
    saved = live.time
    now = req.get("now")
    live.time = types.SimpleNamespace(time=lambda: now)
    try:
        window = req.get("window")
        try:
            return bool(live.timestamp_fresh(req.get("raw"), window))
        finally:
            pass
    finally:
        live.time = saved


def decode_json_list_literal(req):
    return live.decode_json_list_literal(req.get("raw"))


def coerce_port(req):
    return int(live.coerce_port(req.get("value"), req.get("default")))


def bridged_env_marker(req):
    return live.bridged_env_marker(req.get("name"))


def _exc_class(name):
    return type(name, (Exception,), {})


def request_never_left(req):
    """Build the exception objects whose MROs the fixture names, in order."""
    chain = req.get("chain")
    classes = []
    for mro in chain if isinstance(chain, list) else []:
        names = [n for n in mro if isinstance(n, str)] if isinstance(mro, list) else []
        if not names:
            classes.append(None)
            continue
        cls = _exc_class(names[0])
        classes.append(cls)
    nodes = [c() for c in classes if c is not None]
    for idx, node in enumerate(nodes):
        if idx + 1 < len(nodes):
            node.__cause__ = nodes[idx + 1]
    if not nodes:
        return False
    return bool(live.request_never_left(nodes[0]))


def expected(r):
    f = r.get("func")
    fn = {
        "secrets_match": secrets_match,
        "timestamp_fresh": timestamp_fresh,
        "decode_json_list_literal": decode_json_list_literal,
        "coerce_port": coerce_port,
        "bridged_env_marker": bridged_env_marker,
        "request_never_left": request_never_left,
    }.get(f)
    if fn is None:
        raise AssertionError("unknown func %r" % (f,))
    return fn(r)


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
