#!/usr/bin/env python3
"""Differential oracle for plugins/config_backends/remote/client.py
(gate w24_client_64e0a3).

The live client module is imported from the Python quarry.  Nothing here opens a
socket: the transport is the seam, so every case is fed to the same pure helpers
the transport calls --

  * ``_parse_body`` on the raw response bytes;
  * ``_retry_after`` on the Retry-After header text;
  * ``base_url`` / ``instance_id`` with the env var set from the request;
  * ``Response.error`` / ``Response.message`` on a bare dataclass.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_client_64e0a3"

# Import-time code in the live package resolves the user's home; a bare MSYS
# shell exports HOME but no USERPROFILE, which ``Path.home()`` refuses.
if not os.environ.get("USERPROFILE") and not os.environ.get("HOMEPATH"):
    os.environ["USERPROFILE"] = str(REPO.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.config_backends.remote import client as live  # noqa: E402

WANTED = {"parse_body", "retry_after", "base_url", "instance_id",
          "response_error", "response_message", "constants"}


def parse_body(req):
    raw = req.get("text")
    raw = "" if raw is None else raw
    return live._parse_body(raw.encode("utf-8"))


def retry_after(req):
    return live._retry_after(req.get("value"))


def _with_env(env_name, value, fn):
    if value is None:
        os.environ.pop(env_name, None)
    else:
        os.environ[env_name] = value
    try:
        return fn()
    finally:
        os.environ.pop(env_name, None)


def base_url(req):
    return _with_env(live.URL_ENV, req.get("env"), live.base_url)


def instance_id(req):
    return _with_env(live.INSTANCE_ENV, req.get("env"), live.instance_id)


def _response(req):
    body = req.get("body")
    body = body if isinstance(body, dict) else {}
    return live.Response(req.get("status"), body, None, None)


def response_error(req):
    return _response(req).error


def response_message(req):
    return _response(req).message


def constants(_req):
    return {"default_url": live.DEFAULT_URL, "url_env": live.URL_ENV,
            "instance_env": live.INSTANCE_ENV, "http_timeout_s": float(live._HTTP_TIMEOUT_S)}


def expected(r):
    f = r.get("func")
    if f == "parse_body":
        return parse_body(r)
    if f == "retry_after":
        return retry_after(r)
    if f == "base_url":
        return base_url(r)
    if f == "instance_id":
        return instance_id(r)
    if f == "response_error":
        return response_error(r)
    if f == "response_message":
        return response_message(r)
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
