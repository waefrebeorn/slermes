#!/usr/bin/env python3
"""Differential oracle for acp_adapter/catalog.py (gate w6_catalog_c1c3fb).

The storage-facing seams (read_catalog_rows / read_local_owners) are replaced
with the fixture's injected rows/owners; the LIVE catalog_sessions projection
(including its LIVE session helpers) does the comparison.
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

acp = types.ModuleType("acp_adapter")
acp.__path__ = [Q + "/acp_adapter"]
sys.modules["acp_adapter"] = acp

hc = types.ModuleType("hermes_constants")
hc.get_hermes_home = lambda: Q + "/.hermes"
hc.translate_cwd_for_wsl_backend = lambda s: s
hc.windows_path_to_wsl = lambda s: None
sys.modules["hermes_constants"] = hc

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m

session_live = load("acp_adapter.session", Q + "/acp_adapter/session.py")
pa = types.ModuleType("persistence_assembly")
pa.open_persistence_reader = lambda p: None
pa.profile_scope = lambda home: None
pa.store_exists = lambda s: False
sys.modules["persistence_assembly"] = pa
live = load("acp_adapter.catalog", Q + "/acp_adapter/catalog.py")


def sortpy(value):
    p = session_live._updated_at_sort_key(value)
    return "-inf" if p == float("-inf") else p


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
        if func == "catalog_sessions":
            rows = r.get("rows", [])
            owners = r.get("owners", {})
            live.read_catalog_rows = lambda home: {str(row["id"]): dict(row) for row in rows}
            live.read_local_owners = lambda home: owners
            py = live.catalog_sessions(r.get("home", Q))
        elif func == "format_updated_at":
            py = session_live._format_updated_at(r.get("value"))
        elif func == "updated_at_sort_key":
            py = sortpy(r.get("value"))
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

print("ORACLE w6_catalog_c1c3fb: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
