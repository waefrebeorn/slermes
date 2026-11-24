#!/usr/bin/env python3
"""Differential oracle for gateway/relay/http_diagnostics.py
(gate w24_httpdiagno_5c2dc9).

The live module is imported and its value-level helpers are called on the JSON
the harness read.  The seams are supplied as objects: ``connector_reason`` gets
bytes like ``read_error_body`` would hand it, ``answered_by_proxy`` gets a real
``email.message.Message`` (HTTPMessage's case-insensitive ``.get``) wrapped in a
stub exception.  ``describe_http_error``/``redact_excerpt`` are not compared --
their generic redactor is a ~2k-line vendor-pattern module, not a value rule --
so only ``_scrub`` (the exact-credential part) is.
"""
import importlib.util
import json
import sys
from email.message import Message
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_httpdiagno_5c2dc9"
PYFILE = QUARRY / "gateway/relay/http_diagnostics.py"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# The module's only package import is agent.redact (no relative imports), so
# loading the file directly keeps gateway/relay/__init__ out of the way.
_spec = importlib.util.spec_from_file_location("live_http_diagnostics", PYFILE)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

WANTED = {"connector_reason", "answered_by_proxy", "scrub", "constants"}


def connector_reason(r):
    body = r.get("body")
    raw = body.encode("utf-8") if isinstance(body, str) else b""
    return live.connector_reason(raw)


def _message(raw):
    """A headers mapping the way an HTTP response carries it: Message.get is
    case-insensitive, and a missing name is None."""
    if not isinstance(raw, dict):
        return None
    msg = Message()
    for k, v in raw.items():
        msg[str(k)] = "" if v is None else str(v)
    return msg


def answered_by_proxy(r):
    headers = _message(r.get("headers"))
    return live.answered_by_proxy(SimpleNamespace(headers=headers))


def scrub(r):
    text = r.get("text")
    text = text if isinstance(text, str) else ""
    secrets = r.get("secrets")
    secrets = tuple(secrets) if isinstance(secrets, list) else ()
    return live._scrub(text, secrets)


def constants(r):
    return {
        "origin_headers": list(live._ORIGIN_HEADERS),
        "proxy_only_headers": list(live._PROXY_ONLY_HEADERS),
        "proxy_server_names": list(live._PROXY_SERVER_NAMES),
        "body_excerpt_chars": live.BODY_EXCERPT_CHARS,
        "body_read_limit": live._BODY_READ_LIMIT,
    }


DISPATCH = {
    "connector_reason": connector_reason,
    "answered_by_proxy": answered_by_proxy,
    "scrub": scrub,
    "constants": constants,
}


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
            py = DISPATCH[r["func"]](r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
