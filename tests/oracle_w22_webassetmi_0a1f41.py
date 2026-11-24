#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_asset_mime_types.py (gate w22_webassetmi_0a1f41).

The module's whole behaviour is a table plus the pinning loop, and the loop's
effect is observable: after `normalize_web_asset_mime_types()` the live
`mimetypes` map answers the pinned types regardless of the host map. So the
oracle runs the live module, then reads back

  pairs      -- list(WEB_ASSET_MIME_TYPES.items()) in insertion order
  mime_for   -- the dict's own lookup (exact key, no normalisation)
  pinned_map -- mimetypes.types_map after normalize (add_type(strict=True))
  guess      -- mimetypes.guess_type(url)[0] after normalize

`out` is compared as a decoded JSON value, so object key order is not part of
the contract; `pairs` is a list of [ext, mime] pairs and must keep its order.
"""
import importlib.util
import json
import mimetypes
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "web_asset_mime_types.py"
GATE = "w22_webassetmi_0a1f41"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("web_asset_mime_types_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["web_asset_mime_types_live"] = live
_spec.loader.exec_module(live)

live.normalize_web_asset_mime_types()
TABLE = live.WEB_ASSET_MIME_TYPES
WANTED = {"pairs", "mime_for", "pinned_map", "guess"}


def pairs():
    return [[ext, mime] for ext, mime in TABLE.items()]


def mime_for(ext):
    return TABLE.get(ext)


def pinned_map():
    return {ext: mimetypes.types_map[ext] for ext in TABLE}


def guess(url):
    try:
        return mimetypes.guess_type(url)[0]
    except (TypeError, AttributeError):
        # guess_type has no answer for a non-path (the source's TypeError path)
        return None


def expected(r):
    f = r.get("func")
    if f == "pairs":
        return pairs()
    if f == "mime_for":
        return mime_for(r.get("ext"))
    if f == "pinned_map":
        return pinned_map()
    if f == "guess":
        return guess(r.get("url"))
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
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
