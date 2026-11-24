#!/usr/bin/env python3
"""Oracle for the C port of hermes_cli/setup_agent_import.py (gate w17_setupagent_0fd570).

Loads the LIVE module via importlib.util.spec_from_file_location with the
parent-package bootstrap + sys.modules registration, and recomputes the
constants table and the pure string computations embedded in _import_child(),
migrate_from_agents() and offer_agent_import() from live data. Exits 1 on any
mismatch.
"""
import importlib.util
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if not os.path.isfile(os.path.join(Q, "hermes_cli", "setup_agent_import.py")):
    Q = os.path.join(os.path.expanduser("~"), "hermes-agent")
sys.path.insert(0, Q)
SRC = os.path.join(Q, "hermes_cli", "setup_agent_import.py")


def _load_live():
    pkg = "hermes_cli"
    if pkg not in sys.modules:
        pdir = os.path.join(Q, pkg)
        sp = importlib.util.spec_from_file_location(
            pkg, os.path.join(pdir, "__init__.py"), submodule_search_locations=[pdir])
        pm = importlib.util.module_from_spec(sp)
        sys.modules[pkg] = pm
        try:
            sp.loader.exec_module(pm)
        except Exception:
            pass
    name = "hermes_cli.setup_agent_import"
    sp = importlib.util.spec_from_file_location(name, SRC)
    live = importlib.util.module_from_spec(sp)
    sys.modules[name] = live
    sp.loader.exec_module(live)
    return live


live = _load_live()


def source_fields(s):
    return {
        "name": s.name,
        "label": s.label,
        "dirs": list(s.dirs),
        "extra_args": list(s.extra_args),
        "exports": list(s.exports),
    }


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
    if func == "constants":
        py = {
            "PLUGIN": live.PLUGIN,
            "SOURCES": [source_fields(s) for s in live.IMPORT_SOURCES],
        }
    elif func == "argv":
        name = r.get("source", "")
        src = next(s for s in live.IMPORT_SOURCES if s.name == name)
        argv = [r.get("executable", ""), "-P", "-m", "hermes_cli.main"]
        argv += list(r.get("selector") or [])
        argv += ["import-agent", src.name]
        if r.get("from_setup"):
            argv += list(src.extra_args)
        argv += list(r.get("extra_args") or [])
        if r.get("from_setup"):
            argv += ["--from-setup"]
        py = argv
    elif func == "hint":
        pre = ("Found: " + ", ".join(r.get("found") or []) + ". " if r.get("show_found") else
               "No other agent setup found on this computer. ")
        py = pre + "Import one with: " + ", ".join(
            "hermes migrate %s" % s.name for s in live.IMPORT_SOURCES)
    elif func == "context":
        user_home = str(Path(r.get("user_home", "")))
        parts = []
        for e in (r.get("found") or []):
            p = str(Path(e.get("path", ""))).replace(user_home, "~", 1)
            parts.append("%s (%s)" % (e.get("label", ""), p))
        py = "Found on this computer: " + ", ".join(parts) + "."
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)
print("ORACLE w17_setupagent_0fd570: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
