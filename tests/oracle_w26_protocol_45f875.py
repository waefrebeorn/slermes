#!/usr/bin/env python3
"""Differential oracle for agent/lsp/protocol.py (gate w26_protocol_45f875).

The live module is imported as a package module and its pure framer surface is
compared value-for-value.  The async stream is the seam: ``read_message`` is
fed the exact bytes a StreamReader would have produced (``feed_data`` plus
``feed_eof``) and only the parsed envelopes plus the error *kind* are compared,
so the C port has to agree on framing decisions, not on message wording.
"""
import asyncio
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_protocol_45f875"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent.lsp import protocol as live  # noqa: E402

WANTED = {"encode", "make", "classify", "decode", "constants"}

ERROR_KINDS = (
    ("unexpected EOF while reading LSP headers", "eof"),
    ("LSP header block exceeded", "header_block"),
    ("non-ASCII LSP header", "non_ascii_header"),
    ("malformed LSP header line", "malformed_header"),
    ("LSP message missing Content-Length", "missing_content_length"),
    ("non-integer Content-Length", "non_integer_length"),
    ("unreasonable Content-Length", "unreasonable_length"),
    ("truncated LSP body", "truncated_body"),
    ("invalid JSON in LSP body", "invalid_json"),
    ("non-UTF-8 LSP body", "non_utf8"),
)


def error_kind(exc):
    text = str(exc)
    for needle, kind in ERROR_KINDS:
        if text.startswith(needle):
            return kind
    return "other"


def encode(req):
    obj = req.get("obj")
    return live.encode_message(obj).decode("utf-8")


def make(req):
    kind = req.get("kind")
    if kind == "notification":
        return live.make_notification(req.get("method"), req.get("params"))
    if kind == "request":
        return live.make_request(req.get("id"), req.get("method"), req.get("params"))
    if kind == "response":
        return live.make_response(req.get("id"), req.get("result"))
    if kind == "error":
        return live.make_error_response(req.get("id"), req.get("code"),
                                        req.get("message"), req.get("data"))
    raise AssertionError("unknown make kind %r" % (kind,))


def classify(req):
    kind, key = live.classify_message(req.get("msg"))
    return [kind, key]


def decode(req):
    wire = req.get("wire")
    data = wire.encode("utf-8") if isinstance(wire, str) else b""
    messages = []

    async def drain():
        reader = asyncio.StreamReader()
        if data:
            reader.feed_data(data)
        reader.feed_eof()
        while True:
            msg = await live.read_message(reader)
            if msg is None:
                return
            messages.append(msg)

    try:
        asyncio.run(drain())
    except live.LSPProtocolError as exc:
        return {"messages": messages, "error": error_kind(exc)}
    return {"messages": messages, "error": None}


def constants():
    return {
        "error_content_modified": live.ERROR_CONTENT_MODIFIED,
        "error_method_not_found": live.ERROR_METHOD_NOT_FOUND,
        "max_header_bytes": live._MAX_HEADER_BYTES,
        "max_body_bytes": live._MAX_BODY_BYTES,
    }


def expected(r):
    f = r.get("func")
    if f == "encode":
        return encode(r)
    if f == "make":
        return make(r)
    if f == "classify":
        return classify(r)
    if f == "decode":
        return decode(r)
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
