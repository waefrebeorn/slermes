#!/usr/bin/env python3
"""Differential oracle for gateway/stream_consumer_directives.py (w20_streamcons_6b51f8).

The live module is imported straight out of the quarry and its three compiled
patterns answer every case: the comparison is the module's own regexes (and its
``has_pending_media_directive``) against the hand-written scanners in C. The
corpus in the fixture is deliberately adversarial -- anchors, a trailing
newline, quote-run backtracking, multi-line alt text -- because that is where a
hand port and ``re`` differ if they are going to.
"""
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

import gateway.stream_consumer_directives as live  # noqa: E402

GATE = "w20_streamcons_6b51f8"


def expected(r):
    f = r.get("func")
    text = r.get("text")
    if not isinstance(text, str):
        text = ""
    if f == "pending_media":
        return bool(live._PENDING_MEDIA_RE.search(text))
    if f == "pending_flag":
        m = live._PENDING_FLAG_RE.search(text)
        return m.group(0) if m else None
    if f == "flag_opens_stream":
        m = live._PENDING_FLAG_RE.search(text)
        return bool(m) and any(x.startswith(m.group(0)) for x in live._STREAM_FLAGS)
    if f == "pending_image":
        return bool(live._PENDING_IMAGE_RE.search(text))
    if f == "has_pending":
        return bool(live.has_pending_media_directive(text))
    if f == "last_line":
        return text.rsplit("\n", 1)[-1]
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
        if not isinstance(r, dict) or r.get("func") is None:
            continue
        cases += 1
        try:
            exp = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if exp != got:
            mism += 1
            print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
