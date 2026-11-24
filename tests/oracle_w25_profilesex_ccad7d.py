#!/usr/bin/env python3
"""Differential oracle for hermes_cli/profiles_export_scrub.py (gate w25_profilesex_ccad7d).

The export walk is a seam, so the oracle calls the live decision functions on
the same values the port sees:

  * ``_should_redact_export_file`` on real ``Path`` objects (the live code reads
    ``.name``/``.suffix``, so a Windows host and a POSIX host agree on the
    fixtures used here);
  * ``_masked_envelopes`` on parsed documents;
  * ``_mask_secret_envelopes`` on raw text -- this compares the whole re-emitted
    ``json.dumps(indent=2)`` document, not just the masked node;
  * ``GATEWAY_CONNECTION_STORE_RE.fullmatch`` for the saved-gateways stores;
  * the scrub's literal tables, read off the live module.
"""
import importlib.util
import json
import re
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_profilesex_ccad7d"


def load_live():
    path = QUARRY / "hermes_cli" / "profiles_export_scrub.py"
    spec = importlib.util.spec_from_file_location("_live_profiles_export_scrub", path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["_live_profiles_export_scrub"] = mod
    spec.loader.exec_module(mod)
    return mod


live = load_live()

WANTED = {"should_redact", "masked_envelopes", "mask_envelopes_text",
          "gateway_store_match", "constants"}


def should_redact(req):
    path = req.get("path")
    if not isinstance(path, str):
        return None
    return live._should_redact_export_file(Path(path))


def masked_envelopes(req):
    if "node" not in req:
        return None
    return live._masked_envelopes(req["node"])


def mask_envelopes_text(req):
    text = req.get("text")
    if not isinstance(text, str):
        return None
    return live._mask_secret_envelopes(text)


def gateway_store_match(req):
    name = req.get("name")
    if not isinstance(name, str):
        return None
    return live.GATEWAY_CONNECTION_STORE_RE.fullmatch(name) is not None


def constants():
    return {
        "redact_suffixes": sorted(live._EXPORT_REDACT_SUFFIXES),
        "redact_names": sorted(live._EXPORT_REDACT_NAMES),
        "gateway_stores": list(live.GATEWAY_CONNECTION_STORES),
        "gateway_store_pattern": live.GATEWAY_CONNECTION_STORE_RE.pattern,
    }


def expected(req):
    f = req.get("func")
    if f == "should_redact":
        return should_redact(req)
    if f == "masked_envelopes":
        return masked_envelopes(req)
    if f == "mask_envelopes_text":
        return mask_envelopes_text(req)
    if f == "gateway_store_match":
        return gateway_store_match(req)
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
    if cases == 0:
        print("ORACLE %s: no cases consumed" % GATE, file=sys.stderr)
        sys.exit(1)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
