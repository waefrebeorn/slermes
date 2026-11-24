#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/email/adapter.py (gate w28_adapter_ca7894_p2).

The live adapter module is imported BY FILE PATH: its package __init__ pulls the
gateway stack (fastapi/pydantic), and this host's application-control policy
refuses pydantic's compiled extension.  The seam stubs cover that import chain;
the functions under test are pure text transforms of the JSON inputs.

Ported symbols and their live entry points:

  extract_email_address  _extract_email_address(raw)
  domain_of              _domain_of(address)
  domains_aligned        _domains_aligned(a, b)
  ar_clauses             _ar_clauses(text)      -> list | None
  auth_props             _auth_props(text)      -> list[(prop, value)]
"""
import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_adapter_ca7894_p2"

_TMPHOME = tempfile.mkdtemp(prefix="w28-email-home-")
os.environ["HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME

sys.path.append(str(REPO / "tests"))
from hermes_seam_stubs import install as _install  # noqa: E402

_install()
sys.path.insert(0, str(QUARRY))

import importlib.util  # noqa: E402

_SPEC = importlib.util.spec_from_file_location(
    "w28_email_adapter_live", QUARRY / "plugins" / "platforms" / "email" / "adapter.py")
live = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(live)

WANTED = {"extract_email_address", "domain_of", "domains_aligned", "ar_clauses", "auth_props"}


def expected(r):
    f = r.get("func")
    if f == "extract_email_address":
        return live._extract_email_address(r.get("raw") or "")
    if f == "domain_of":
        return live._domain_of(r.get("address") or "")
    if f == "domains_aligned":
        return live._domains_aligned(r.get("a") or "", r.get("b") or "")
    if f == "ar_clauses":
        got = live._ar_clauses(r.get("text") or "")
        return None if got is None else list(got)
    if f == "auth_props":
        return [list(pair) for pair in live._auth_props(r.get("text") or "")]
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
    if cases == 0:
        print("ORACLE %s: FAIL-CLOSED, no cases" % GATE, file=sys.stderr)
        sys.exit(2)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
