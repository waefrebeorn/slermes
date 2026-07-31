#!/usr/bin/env python3
import os
"""
sta_oracle_msg_sanitize_surrogates.py — oracle for
t_port_msg_sanitize_surrogates.c.

Recomputes each case from the LIVE agent/message_sanitization.py
(_sanitize_structure_surrogates) and emits one JSON line per case (same
shape as the C harness). The runner diffs them byte-for-byte.

The Python function mutates the payload in-place and returns whether any
surrogates were replaced; we serialize the (mutated) payload as "out"
and the returned bool as "found", matching the C harness exactly.
"""
import json
import sys
import importlib.util


def _load():
    # Deterministic LIVE-Python resolution: prefer the canonical dev repo
    # (parent of slermes/) over any installed/stale copy on sys.path
    # (e.g. ~/.hermes/hermes-agent), which would manufacture false FAPs.
    _repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if _repo not in sys.path:
        sys.path.insert(0, _repo)
    for base in sys.path:
        cand = f"{base}/agent/message_sanitization.py"
        try:
            spec = importlib.util.spec_from_file_location("live_ms", cand)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            continue
    import agent.message_sanitization as mod  # type: ignore
    return mod


ms = _load()


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_msg_sanitize_surrogates.py <trees.json>\n")
        return 2
    with open(sys.argv[1], "rb") as f:
        raw = f.read()
    # The fixture carries raw UTF-8 surrogate bytes (as they arrive from a
    # reasoning model). Decode leniently so surrogates survive, then parse
    # with strict=False (lone surrogates are valid for our scrubber).
    text = raw.decode("utf-8", errors="surrogatepass")
    trees = json.loads(text, strict=False)
    for tree in trees:
        payload = json.loads(json.dumps(tree))  # deep copy
        found = ms._sanitize_structure_surrogates(payload)
        rec = {
            "out": payload,
            "found": bool(found),
        }
        sys.stdout.write(json.dumps(rec, ensure_ascii=False, separators=(",", ":")) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
