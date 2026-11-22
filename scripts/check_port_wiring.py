#!/usr/bin/env python3
"""Check whether a C port is wired beyond its own harness.

Usage: python scripts/check_port_wiring.py src/cli/gateway_windows_pure.c

Exit 0 = the object is registered in build/objects.mk AND at least one .c
outside tests/t_port_*.c references one of its exported symbols. Exit 1 =
body-verified but harness-only (not product-integrated). This is deliberately
strict: a PoP comment is not a caller.
"""
import os
import re
import sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = sys.argv[1] if len(sys.argv) > 1 else ""
if not src:
    print("usage: check_port_wiring.py <port.c>")
    sys.exit(2)
rel = os.path.relpath(os.path.abspath(src), root).replace("\\", "/")
if rel.startswith("../"):
    print(f"not in repo: {src}")
    sys.exit(2)

objs = open(os.path.join(root, "build/objects.mk"), encoding="utf-8", errors="replace").read()
obj = rel[:-2] + ".o"
registered = obj in objs

text = open(os.path.join(root, rel), encoding="utf-8", errors="replace").read()
syms = set(re.findall(r'^(?![A-Za-z_ ]*\bstatic\b)[A-Za-z_][\w *]*?\b([A-Za-z_]\w*)\s*\([^;]*\)\s*\{', text, re.M))
syms -= {"if", "for", "while", "switch", "return", "sizeof"}
callers = []
for dp, dn, fn in os.walk(os.path.join(root, "src")):
    for f in fn:
        if not f.endswith(".c"):
            continue
        p = os.path.join(dp, f)
        r = os.path.relpath(p, root).replace("\\", "/")
        if r == rel or r.startswith("tests/"):
            continue
        body = open(p, encoding="utf-8", errors="replace").read()
        if any(re.search(r'\b' + re.escape(s) + r'\s*\(', body) for s in syms):
            callers.append(r)

print(f"port: {rel}")
print(f"registered in build/objects.mk: {'yes' if registered else 'NO'}")
print(f"exported functions: {len(syms)}")
print(f"production callers: {len(callers)}")
for c in callers[:8]:
    print(f"  {c}")
if not registered or not callers:
    print("VERDICT: harness-only / unwired — do not claim product parity")
    sys.exit(1)
print("VERDICT: wired")
