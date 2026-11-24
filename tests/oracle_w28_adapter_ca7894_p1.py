#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/email/adapter.py (gate w28_adapter_ca7894_p1).

The live adapter module is imported BY FILE PATH: its package __init__ pulls the
gateway stack (fastapi/pydantic), and this host's application-control policy
refuses pydantic's compiled extension.  The seam stubs cover that import chain;
nothing under test needs it -- every compared value is a pure function of the
JSON inputs the harness also feeds the C port.

Ported symbols and their live entry points:

  normalize_security   _normalize_security(value, default)
  is_automated_sender  _is_automated_sender(address, headers)
  safe_decode          _safe_decode(payload, charset)          payload = fixture base64
  decode_header_value  _decode_header_value(raw)
  strip_html           _strip_html(html)
  strip_comments       _strip_comments(text)
"""
import base64
import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_adapter_ca7894_p1"

# The quarry resolves its Hermes home at import time; hand it a throwaway one so
# nothing here can read or write the operator's real state.
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

WANTED = {
    "normalize_security", "is_automated_sender", "safe_decode",
    "decode_header_value", "strip_html", "strip_comments",
}


def expected(r):
    f = r.get("func")
    if f == "normalize_security":
        if "default" in r:
            return live._normalize_security(r.get("value"), r.get("default"))
        return live._normalize_security(r.get("value"))
    if f == "is_automated_sender":
        headers = r.get("headers")
        return live._is_automated_sender(r.get("address") or "",
                                         headers if isinstance(headers, dict) else {})
    if f == "safe_decode":
        raw = r.get("payload_b64") or ""
        payload = base64.b64decode(raw) if raw else b""
        return live._safe_decode(payload, r.get("charset"))
    if f == "decode_header_value":
        return live._decode_header_value(r.get("raw") or "")
    if f == "strip_html":
        return live._strip_html(r.get("html") or "")
    if f == "strip_comments":
        return live._strip_comments(r.get("text") or "")
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
