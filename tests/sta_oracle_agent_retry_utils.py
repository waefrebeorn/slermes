#!/usr/bin/env python3
"""sta_oracle_agent_retry_utils.py — oracle for
agent/retry_utils.py vs LIVE Python C harness.

Reads the same cases.in fixture the C harness consumed (path in argv[1]);
imports the LIVE agent.retry_utils module; recomputes the matching
function for each case; emits one JSON line per case:
    {"func":"...","...":"...","out":"..."}

run_oracle.sh pipes the C harness's JSONL on stdin (ignored here) and diffs
this stdout against the C output byte-for-byte (after normalization).
"""
import json
import os
import sys

_repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _repo not in sys.path:
    sys.path.insert(0, _repo)

from agent.retry_utils import (
    is_zai_coding_overload_error,
    parse_retry_after_seconds,
)


class Err:
    """Minimal duck-typed error object matching what Python reads.
     Attribute access for .message/.body/.response returns the lowercased
     text (which is what _error_text ultimately joinS)."""
    def __init__(self, status, text):
        self.status_code = status
        self._t = text
    def __getattr__(self, name):
        if name in ("message", "body", "response"):
            return self._t
        raise AttributeError(name)


def run_is_zai(case):
    err = Err(case["status"], case.get("text", ""))
    out = bool(is_zai_coding_overload_error(
        base_url=case.get("base"),
        model=case.get("model"),
        error=err,
    ))
    return out


def run_parse_retry_after(case):
    if "value" in case:
        py_input = case["value"]
    elif "numeric" in case and case["numeric"] is not None:
        py_input = case["numeric"]
    else:
        py_input = None
    out = parse_retry_after_seconds(py_input)
    return out


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_agent_retry_utils.py <cases.in>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        cases = json.load(f)

    for c in cases:
        func = c["func"]
        if func == "is_zai_coding_overload_error":
            out = run_is_zai(c)
            sys.stdout.write(json.dumps(
                {"func": func, "code": c["status"],
                 "base": c.get("base"), "model": c.get("model"),
                 "text": c.get("text"), "out": int(out)},
                ensure_ascii=False, separators=(",", ":")) + "\n")
        elif func == "parse_retry_after_seconds":
            out = run_parse_retry_after(c)
            if out is None:
                sys.stdout.write(json.dumps(
                    {"func": func, "value": c.get("value"),
                     "numeric": c.get("numeric"), "ok": 0, "out": None},
                    ensure_ascii=False, separators=(",", ":")) + "\n")
            else:
                sys.stdout.write(json.dumps(
                    {"func": func, "value": c.get("value"),
                     "numeric": c.get("numeric"), "ok": 1,
                     "out": out},
                    ensure_ascii=False, separators=(",", ":")) + "\n")
        else:
            sys.stderr.write("unknown func %r\n" % func)
            return 3
    return 0


if __name__ == "__main__":
    sys.exit(main())
