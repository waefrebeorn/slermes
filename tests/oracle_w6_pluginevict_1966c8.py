#!/usr/bin/env python3
"""Differential oracle for pm/plugin_eviction.py (gate w6_pluginevict_1966c8).

Only the pure dict/list decision core is compared: _discard, _list,
_prune_record, settle_record and _outcome_notices. Filesystem / resolver /
publication side effects are out of scope and never reached by these cases.
"""
import copy
import importlib.util
import json
import sys
from pathlib import Path

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

SRC = QUARRY + "/pm/plugin_eviction.py"
spec = importlib.util.spec_from_file_location("w6_pluginevict_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def _entry(raw):
    return (Path(raw[0]), raw[1], Path(raw[2]))


def compute(r):
    func = r.get("func")
    if func == "discard":
        names = copy.deepcopy(r.get("names", []))
        live._discard(names, r.get("spellings", []))
        return names
    if func == "list":
        plugins = copy.deepcopy(r.get("plugins", {}))
        value = live._list(plugins, r.get("key", "disabled"))
        return [value, plugins]
    if func == "prune":
        plugins = copy.deepcopy(r.get("plugins", {}))
        live._prune_record(plugins)
        return plugins
    if func == "settle":
        plugins = copy.deepcopy(r.get("plugins", {}))
        live.settle_record(plugins, r.get("settled", []))
        return plugins
    if func == "notices":
        f = r.get("facts", {})
        entries = [_entry(e) for e in f.get("entries", [])]
        recorded = {_entry(p["entry"]): p["record"] for p in f.get("recorded", [])}
        reasons = {Path(k).resolve(): v for k, v in f.get("reasons", {}).items()}
        waiting = {Path(k).resolve(): v for k, v in f.get("waiting", {}).items()}
        joined = [_entry(e) for e in f.get("joined", [])]
        return live._outcome_notices(entries, recorded, reasons, waiting, joined)
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s %r" % (r.get("func"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)

print("ORACLE w6_pluginevict_1966c8: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
