#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_server_remote_media.py (gate w25_webserverr_68f039).

The live module is imported and its pure range arithmetic is called directly;
``remote_media_response`` is driven with a synthetic ASGI ``Request`` and a
workspace adapter that records the slice it is asked for -- that adapter call is
the seam (an SSH file has no local handle), so what is compared is the range
grammar, the 416 case and the response envelope.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_webserverr_68f039"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from fastapi import HTTPException  # noqa: E402
from starlette.requests import Request  # noqa: E402

from hermes_cli import web_server_remote_media as live  # noqa: E402

WANTED = {"position", "byte_range", "response", "constants"}


class Recorder:
    """The workspace adapter seam: records the slice, yields one chunk."""

    def __init__(self):
        self.calls = []

    def stream_file(self, target, offset=None, length=None):
        self.calls.append((target, offset, length))
        return iter([b"x"])


def _request(spec):
    spec = spec if isinstance(spec, dict) else {}
    headers = []
    raw = spec.get("headers")
    if isinstance(raw, dict):
        for key, value in raw.items():
            if isinstance(value, str):
                headers.append((key.lower().encode("latin-1"), value.encode("latin-1")))
    scope = {
        "type": "http",
        "method": spec.get("method", "GET") if "method" in spec else "GET",
        "headers": headers,
        "query_string": b"",
        "path": "/api/files/stream",
    }
    return Request(scope)


def position(req):
    return live._position(req.get("digits") if isinstance(req.get("digits"), str) else "")


def byte_range(req):
    header = req.get("header")
    header = header if isinstance(header, str) else None
    size = req.get("size")
    size = size if isinstance(size, int) and not isinstance(size, bool) else 0
    try:
        span = live.byte_range(header, size)
    except HTTPException:
        return {"unsatisfiable": size}
    if span is None:
        return None
    return {"first": span[0], "last": span[1]}


def response(req):
    size = req.get("size")
    size = size if isinstance(size, int) and not isinstance(size, bool) else 0
    target = req.get("target") if isinstance(req.get("target"), str) else ""
    media_type = req.get("media_type") if isinstance(req.get("media_type"), str) else ""
    recorder = Recorder()
    try:
        resp = live.remote_media_response(_request(req.get("request")), recorder, target, size, media_type)
    except HTTPException:
        return {"unsatisfiable": size}
    read = None
    if recorder.calls:
        _target, offset, length = recorder.calls[-1]
        read = "full" if offset is None else {"offset": offset, "length": length}
    return {
        "status": resp.status_code,
        "headers": {k.lower(): v for k, v in resp.headers.items()},
        "read": read,
    }


def constants():
    return {
        "max_position_digits": live._MAX_POSITION_DIGITS,
        "past_any_file": live._PAST_ANY_FILE,
    }


def expected(r):
    f = r.get("func")
    if f == "position":
        return position(r)
    if f == "byte_range":
        return byte_range(r)
    if f == "response":
        return response(r)
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
