#!/usr/bin/env python3
"""Differential oracle for agent/conversation_compression_image_shrink.py
(gate w26_conversati_97cb0b).

The live helpers are imported and called directly.  The two seams are injected:
``_decode_pixels`` runs the real Pillow probe on the payload (so the C port's
PNG probe is compared against Pillow's answer) and ``_shrink_data_url`` gets a
``resize_fn`` that returns the case's pre-computed re-encode, exactly the value
the real resizer would hand back.  The temp-file write inside the live shrink
path is real and discarded, as in production.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_conversati_97cb0b"

import os  # noqa: E402
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent import conversation_compression_image_shrink as live  # noqa: E402

WANTED = {"data_url_mime", "source_to_data_url", "write_source", "decode_pixels",
          "shrink", "constants"}


def data_url_mime(req):
    default = req.get("default")
    if default is None:
        return live._data_url_mime(req.get("header"))
    return live._data_url_mime(req.get("header"), default)


def source_to_data_url(req):
    return live._source_to_data_url(req.get("source"))


def write_source(req):
    return live._write_data_url_to_source(req.get("source"), req.get("data_url") or "")


def decode_pixels(req):
    got = live._decode_pixels(req.get("data_url"))
    return None if got is None else [got[0], got[1]]


def shrink(req):
    resized = req.get("resized")

    def resize_fn(path, *, mime_type=None, max_base64_bytes=None, max_dimension=None):
        return resized

    url = req.get("url")
    max_dimension = req.get("max_dimension")
    out = live._shrink_data_url(url, max_dimension=int(max_dimension or 0),
                                resize_fn=resize_fn)
    return [out[0], out[1]]


def constants():
    return {
        "shrink_target_bytes": live._IMAGE_SHRINK_TARGET_BYTES,
        "default_max_dimension":
            live.try_shrink_image_parts_in_messages.__kwdefaults__.get("max_dimension"),
        "suffix_by_mime": dict(live._IMAGE_SUFFIX_BY_MIME),
    }


def expected(r):
    f = r.get("func")
    if f == "data_url_mime":
        return data_url_mime(r)
    if f == "source_to_data_url":
        return source_to_data_url(r)
    if f == "write_source":
        return write_source(r)
    if f == "decode_pixels":
        return decode_pixels(r)
    if f == "shrink":
        return shrink(r)
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
