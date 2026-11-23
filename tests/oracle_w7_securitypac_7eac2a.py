#!/usr/bin/env python3
"""Differential oracle for pm/security_packages.py (gate w7_securitypac_7eac2a).

Loads the LIVE module via importlib.util.spec_from_file_location and compares
the pure URL builders. No filesystem/network work is exercised.
"""
import importlib.util
import json
import sys
import types

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

# Parent-package bootstrap: let the real pm/__init__.py run so transitive
# imports resolve, then spec-load the exact source file under test.
import pm  # noqa: F401
spec = importlib.util.spec_from_file_location("pm.security_packages", Q + "/pm/security_packages.py")
live = importlib.util.module_from_spec(spec)
sys.modules["pm.security_packages"] = live
spec.loader.exec_module(live)

bws = live.Bws()
iron = live.IronProxy()

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    try:
        if func == "bws_fetch_url":
            py = bws.fetch_url(r.get("version", ""), r.get("target", ""))
        elif func == "iron_fetch_url":
            py = iron.fetch_url(r.get("version", ""), r.get("target", ""))
        elif func == "iron_fetch_urls":
            py = iron.fetch_urls(r.get("version", ""), r.get("target", ""))
        else:
            continue
    except Exception as exc:
        print("LIVE ERROR %s %r: %r" % (func, r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w7_securitypac_7eac2a: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
