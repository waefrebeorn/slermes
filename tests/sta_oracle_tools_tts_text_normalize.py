#!/usr/bin/env python3
"""Faithfulness oracle for tests/t_port_tools_tts_text_normalize.c.

Reads the same cases.in fixture the C harness consumed (path in argv[1]);
imports the LIVE tools/tts_text_normalize module; recomputes the matching
function for each {func, in} case; emits one JSON line per case:
    {"func":"...","in":"...","out":"..."}

run_oracle.sh pipes the C harness's JSONL on stdin (ignored here) and diffs
this stdout against the C output byte-for-byte.
"""
import json
import os
import sys


def _load():
    # Deterministic LIVE-Python resolution: prefer the canonical dev repo
    # (parent of slermes/) over any installed/stale copy on sys.path, which
    # would manufacture false FAPs. The runner also symlinked
    # ~/.hermes/hermes-agent -> DEV_ROOT for isolation, so this is robust.
    _repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if _repo not in sys.path:
        sys.path.insert(0, _repo)
    import tools.tts_text_normalize as mod  # type: ignore
    return mod


tp = _load()

# Map the fixture's portable name to the real Python function object.
DISPATCH = {
    "strip_markdown_for_tts":        tp.strip_markdown_for_tts,
    "normalize_symbols_for_tts":     tp.normalize_symbols_for_tts,
    "_normalize_temperature_ranges": tp._normalize_temperature_ranges,
    "smooth_whitespace_for_tts":     tp.smooth_whitespace_for_tts,
    "strip_nonspoken_blocks":        tp.strip_nonspoken_blocks,
    "flatten_newlines_for_payload":  tp.flatten_newlines_for_payload,
    "prepare_spoken_text":           tp.prepare_spoken_text,
}


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_tools_tts_text_normalize.py <cases.in>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        cases = json.load(f)

    for c in cases:
        func = c["func"]
        inp = c["in"]
        fn = DISPATCH.get(func)
        if fn is None:
            sys.stderr.write("unknown func %r\n" % func)
            return 3
        if func == "prepare_spoken_text":
            out = fn(inp, 4000)       # default max_chars, matching the C harness
        else:
            out = fn(inp)
        sys.stdout.write(json.dumps(
            {"func": func, "in": inp, "out": out},
            ensure_ascii=False, separators=(",", ":")) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
