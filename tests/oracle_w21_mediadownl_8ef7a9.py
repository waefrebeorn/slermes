#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/matrix/media_download.py
(gate w21_mediadownl_8ef7a9).

The live module is pure stdlib (time + typing), so `read_body_capped` is
executed for real: the fixture's chunk list is replayed through an async
generator exactly as mautrix's `iter_chunked()` would.  `download_mxc_capped`
needs a live client/HTTP session and stays upstream.

Body bytes are compared as lowercase hex (the wire is bytes, not text); the
fixture keeps chunks ASCII so Python's `len(chunk)` and the C port's byte
length agree.
"""
import asyncio
import importlib
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "matrix" / "media_download.py"
GATE = "w21_mediadownl_8ef7a9"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# Loaded by path, not by package name: plugins/platforms/matrix/__init__.py imports
# the mautrix adapter, which needs a real HERMES home (the gate runs with an
# isolated temp HOME).  This leaf module only needs stdlib.
_spec = importlib.util.spec_from_file_location("matrix_media_download_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["matrix_media_download_live"] = live
_spec.loader.exec_module(live)


async def _aiter(chunks):
    # The fixture carries bytes as JSON strings; the harness hands them to
    # read_body_capped as the bytes mautrix's iter_chunked() would yield.
    for chunk in chunks:
        yield chunk.encode("utf-8") if isinstance(chunk, str) else chunk


def expected(r):
    f = r.get("func")
    if f == "chunk_size":
        return live._CHUNK_SIZE
    headers = r.get("headers")
    chunks = r.get("chunks")
    limit = r.get("limit")
    gen = _aiter(chunks) if isinstance(chunks, list) else chunks
    try:
        body = asyncio.run(live.read_body_capped(headers, gen, limit))
    except Exception as exc:  # noqa: BLE001 - reported as the harness shapes it
        return {"status": "error", "type": type(exc).__name__, "message": str(exc)}
    return {"status": "ok", "size": str(len(body)), "body_hex": bytes(body).hex()}


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
        if not isinstance(r, dict) or r.get("func") not in ("read", "chunk_size"):
            continue
        cases += 1
        py = expected(r)
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
