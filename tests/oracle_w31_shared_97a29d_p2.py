#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/_shared.py delivery/payload shaping
(gate w31_shared_97a29d_p2).

The live module is imported straight out of the quarry.  Each pure helper is
called with the fixture's values; the seams are injected, never exercised:

  * ``delivery_state`` / ``delivered_message_ids`` are pure over a result dict;
  * ``DeliveryOutcome`` is instantiated directly and its state fields filled
    from the fixture (no sender, no transport), then ``failure`` /
    ``split_fields`` run;
  * ``_YAML_KINDS[kind][0]`` is the applies-when predicate itself;
  * ``apply_yaml_bridge`` is called with ``yaml_env_setter`` replaced by a
    no-op, so what is compared is the ``seeded`` extra it returns -- the env
    write (and the ``json`` kind's json.dumps encode) is the seam.

``ids`` are compared as the live code builds them: ``delivered_message_ids``
stringifies, so a numeric id lands in the envelope as its ``str()``.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w31_shared_97a29d_p2"

# The quarry resolves its data dir from the OS home at import time; a bare
# ``bash -lc`` environment has none, so pin it to the repo's own parent.
if not os.environ.get("HOME") and not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)
    os.environ["HOME"] = str(QUARRY.parent)
os.environ.setdefault("LOCALAPPDATA", str(QUARRY.parent / "AppData" / "Local"))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import gateway.platforms._shared as live  # noqa: E402

WANTED = {"delivery_state", "delivered_message_ids", "outcome_failure", "split_fields",
          "yaml_kind_applies", "yaml_bridge_seed"}


def delivery_state(req):
    return live.delivery_state(req.get("result"))


def delivered_message_ids(req):
    return live.delivered_message_ids(req.get("result"))


def outcome(req):
    """One logical send's history, exactly as a sender would have marked it."""
    o = live.DeliveryOutcome()
    ids = req.get("message_ids")
    o.message_ids = [i for i in ids if i is not None] if isinstance(ids, list) else []
    o.has_landed = bool(req.get("has_landed"))
    o.is_ambiguous = bool(req.get("is_ambiguous"))
    return o


def outcome_failure(req):
    error = req.get("error")
    error = dict(error) if isinstance(error, dict) else {}
    return outcome(req).failure(error, never_sent=bool(req.get("never_sent")))


def split_fields(req):
    return outcome(req).split_fields()


def yaml_kind_applies(req):
    kind = req.get("kind")
    applies = live._YAML_KINDS[kind][0]
    cfg = req.get("cfg")
    return bool(applies(cfg if isinstance(cfg, dict) else {}, req.get("key")))


def yaml_bridge_seed(req):
    """apply_yaml_bridge with the env write replaced by a no-op sink."""
    saved = live.yaml_env_setter
    live.yaml_env_setter = lambda **kw: (lambda name, value: None)
    try:
        cfg = req.get("cfg")
        spec = req.get("spec")
        rows = [tuple(r) for r in spec if isinstance(r, list) and len(r) >= 3] \
            if isinstance(spec, list) else []
        return live.apply_yaml_bridge(cfg if isinstance(cfg, dict) else {}, rows)
    finally:
        live.yaml_env_setter = saved


def expected(r):
    f = r.get("func")
    fn = {
        "delivery_state": delivery_state,
        "delivered_message_ids": delivered_message_ids,
        "outcome_failure": outcome_failure,
        "split_fields": split_fields,
        "yaml_kind_applies": yaml_kind_applies,
        "yaml_bridge_seed": yaml_bridge_seed,
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
