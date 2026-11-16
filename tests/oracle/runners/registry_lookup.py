#!/usr/bin/env python3
"""One registry lookup per port, in shell-safe form.

The runner read the registry three times per port (`harness`, `status`,
`incomplete_reason`) -- three interpreter startups at ~200 ms each to answer
questions about the same JSON object.

Prints shlex-quoted assignments; the caller evals them.
"""
import json
import shlex
import sys

name = sys.argv[1]
try:
    with open("tests/oracle/registry.json", encoding="utf-8") as fh:
        reg = json.load(fh)
except Exception:
    reg = {}

port = (reg.get("ports", {}) or {}).get(name)
if not isinstance(port, dict):
    port = {}

print("REG_HARNESS=%s" % shlex.quote(port.get("harness", "") or ""))
print("REG_STATUS=%s" % shlex.quote(port.get("status", "") or ""))
print("REG_REASON=%s" % shlex.quote(port.get("incomplete_reason", "") or ""))
