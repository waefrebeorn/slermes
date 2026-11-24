#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/matrix/adapter.py (gate p1).

The live adapter module is imported through its package (its relative imports
need the package context).  Only its pure helpers are driven: the mautrix
client, aiohttp sessions, the crypto store and every coroutine stay seamed.

Sets are compared as sorted lists, tuples as lists -- the shapes the C port
returns through JSON.
"""
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_e9cb6b_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# The adapter package drags agent.* imports that resolve a home directory and a
# Hermes home at import time; a login shell here has neither, so pin both to the
# repo parent / a scratch dir instead of guessing.
import tempfile  # noqa: E402
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("HERMES_HOME", tempfile.mkdtemp(prefix="w27-oracle-home-"))

# Fixtures define the inputs; ambient MATRIX_* must not leak into them.
for _k in ("MATRIX_MAX_MESSAGE_LENGTH", "MATRIX_E2EE_MODE", "MATRIX_ENCRYPTION"):
    os.environ.pop(_k, None)

from plugins.platforms.matrix import adapter as live  # noqa: E402

WANTED = {
    "bang_split", "bang_candidates", "reply_fallback_extract", "reply_fallback_strip",
    "reply_fallback_split", "permanent_auth_error", "utf8_len", "max_message_length",
    "looks_like_transport_filename", "is_bare_media_filename",
    "event_timestamp_seconds", "redact_url_for_log",
}


class _Cfg:
    def __init__(self, extra):
        self.extra = extra


class _Exc:
    def __init__(self, **kw):
        for k, v in kw.items():
            setattr(self, k, v)


def _bang_split(r):
    m = live._MATRIX_BANG_COMMAND_RE.match(r["text"])
    return None if m is None else {"name": m.group(1), "rest": m.group(2) or ""}


def _bang_candidates(r):
    name = r["name"]
    return list(dict.fromkeys((name.lower(), name.lower().replace("_", "-"))))


def _reply_extract(r):
    quoted, author = live._extract_reply_fallback(r.get("body"))
    return [quoted, author]


def _reply_strip(r):
    return live._strip_reply_fallback(r.get("body"))


def _reply_split(r):
    head, tail = live._split_reply_fallback(r.get("body"))
    return [head, tail]


def _auth_error(r):
    kw = {}
    if r.get("errcode") is not None:
        kw["errcode"] = r["errcode"]
    if r.get("http_status") is not None:
        kw["http_status"] = r["http_status"]
    return live._is_permanent_matrix_auth_error(_Exc(**kw))


def _utf8(r):
    return live._utf8_len(r["text"])


def _maxlen(r):
    raw = r.get("raw")
    cfg = _Cfg({} if raw is None else {"max_message_length": raw})
    return live._resolve_max_message_length(cfg)


def _transport(r):
    if r.get("kind") == "image":
        prefixes, exts = "image/", live._MATRIX_IMAGE_FILENAME_EXTS
    else:
        prefixes, exts = ("audio/", "video/"), live._MATRIX_MEDIA_FILENAME_EXTS
    return live._looks_like_transport_filename(
        r.get("text"), prefixes, exts, bool(r.get("reject_spaces")))


def _bare_media(r):
    return live._is_bare_media_filename(r.get("msgtype"), r.get("body"))


def _ts(r):
    ev = type("E", (), {"timestamp": r.get("ts")})()
    return live._matrix_event_timestamp_seconds(ev)


def _redact(r):
    return live._redact_url_for_log(r.get("url"))


CASES = {
    "bang_split": _bang_split,
    "bang_candidates": _bang_candidates,
    "reply_fallback_extract": _reply_extract,
    "reply_fallback_strip": _reply_strip,
    "reply_fallback_split": _reply_split,
    "permanent_auth_error": _auth_error,
    "utf8_len": _utf8,
    "max_message_length": _maxlen,
    "looks_like_transport_filename": _transport,
    "is_bare_media_filename": _bare_media,
    "event_timestamp_seconds": _ts,
    "redact_url_for_log": _redact,
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
            py = CASES[r["func"]](r)
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
