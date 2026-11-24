#!/usr/bin/env python3
"""Differential oracle for gateway/relay/adapter_inbound_media.py
(gate w21_adapterinb_940828).

The live source is executed straight from the AST: `localize_inbound_media`
(async) and the real `pad_inline_flags` from
gateway/platforms/base_media_flags.py (it imports MessageEvent at module level,
which would drag the gateway package in, so the function itself is lifted --
its two getattr calls only need the event object). Only the connector media
client is stubbed: `download_with_name` (the network download) and
`is_relay_media_url` (origin classification) answer from the fixture. The
logger is a sink.

`raised` is observed, not assumed: on the live `except Exception` path the
event attributes are never reassigned, so the object identity of media_urls
tells us whether the localization was abandoned.
"""
import ast
import asyncio
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "gateway" / "relay" / "adapter_inbound_media.py"
FLAGS_SRC = QUARRY / "gateway" / "platforms" / "base_media_flags.py"
GATE = "w21_adapterinb_940828"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

KEYS = ("media_urls", "media_types", "media_filenames", "media_text_inlined")


class _Sink:
    def debug(self, *a, **kw):
        pass


G = {"__name__": "adapter_inbound_media_live", "logger": _Sink(),
     "Optional": object, "list": list}


def _exec(path, names, into):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names:
            node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec"), into)


def load():
    _exec(FLAGS_SRC, {"pad_inline_flags"}, G)
    _exec(SRC, {"localize_inbound_media"}, G)
    missing = [n for n in ("pad_inline_flags", "localize_inbound_media") if n not in G]
    if missing:
        raise SystemExit("oracle: live source drifted, missing %s" % ", ".join(missing))


class _Event:
    pass


class _Client:
    def __init__(self, downloads, relay):
        self._downloads = downloads
        self._relay = set(relay)

    async def download_with_name(self, url, suggested_name=None):
        got = self._downloads.get(url)
        if got == "__raise__":
            raise RuntimeError("download failed for %s" % url)
        if not got:
            return None
        return (got[0], got[1])

    def is_relay_media_url(self, url):
        return url in self._relay


def _event(state):
    ev = _Event()
    for key in KEYS:
        if key in state and state[key] is not None:
            setattr(ev, key, list(state[key]))
        elif key in state:
            setattr(ev, key, None)
    return ev


def expected(r):
    load()
    f = r.get("func")
    if f == "pad_flags":
        ev = _event(r)
        return G["pad_inline_flags"](ev)
    if f != "localize":
        raise AssertionError("unknown func %r" % (f,))

    ev = _event(r)
    before = getattr(ev, "media_urls", None)
    downloads = r.get("downloads") or {}
    relay = r.get("relay_urls") or []
    client = _Client(downloads, relay) if r.get("has_client") else None

    def get_client():
        if r.get("client_raises"):
            raise RuntimeError("relay client unavailable")
        return client

    asyncio.run(G["localize_inbound_media"](ev, get_client))
    out = {}
    for key in KEYS:
        out[key] = getattr(ev, key, None)
    # The success path always reassigns all four attributes; the swallowed
    # exception path leaves the originals in place.
    had_urls = bool(r.get("media_urls"))
    out["raised"] = bool(had_urls and getattr(ev, "media_urls", None) is before)
    return out


def main():
    cases = mism = 0
    wanted = {"localize", "pad_flags"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
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
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
