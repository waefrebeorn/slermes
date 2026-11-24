#!/usr/bin/env python3
"""Oracle for the C port of hermes_cli/web_server_surface.py (gate w17_webserver_7f8b25).

Loads the LIVE module from the quarry via importlib.util.spec_from_file_location
(with parent-package bootstrap + sys.modules registration), drives surface(),
policy() and private_launch() on a SimpleNamespace state, and diffs the harness
`out` against the live result. Exits 1 on any mismatch.
"""
import dataclasses
import importlib.util
import json
import os
import sys
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if not os.path.isfile(os.path.join(Q, "hermes_cli", "web_server_surface.py")):
    Q = os.path.join(os.path.expanduser("~"), "hermes-agent")
sys.path.insert(0, Q)
SRC = os.path.join(Q, "hermes_cli", "web_server_surface.py")


def _load_live():
    # Parent-package bootstrap: register hermes_cli as a package without running
    # its (heavy) __init__, so the live submodule resolves under its real name.
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
    name = "hermes_cli.web_server_surface"
    sp = importlib.util.spec_from_file_location(name, SRC)
    live = importlib.util.module_from_spec(sp)
    sys.modules[name] = live
    sp.loader.exec_module(live)
    return live


live = _load_live()


def state_of(r):
    return SimpleNamespace(
        ui_surface=r.get("ui_surface"),
        gateway_runner=(object() if r.get("gateway_runner") else None),
        auth_required=bool(r.get("auth_required", False)),
    )


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
    st = state_of(r)
    if func == "surface":
        py = live.surface(st)
    elif func == "policy":
        py = dataclasses.asdict(live.policy(st))
    elif func == "private_launch":
        py = live.private_launch(st)
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)
print("ORACLE w17_webserver_7f8b25: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
