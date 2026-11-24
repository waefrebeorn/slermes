#!/usr/bin/env python3
"""Oracle for the C port of hermes_cli/update_cmd_windows.py (gate w17_updatecmd_736da6).

Loads the LIVE module via importlib.util.spec_from_file_location with the
parent-package bootstrap + sys.modules registration, and drives the real
_planned_stop_marker_path, _lower_dir_prefix and _pending_relaunch_pids (the
latter with an injected pid_exists lambda). Exits 1 on any mismatch.
"""
import importlib.util
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if not os.path.isfile(os.path.join(Q, "hermes_cli", "update_cmd_windows.py")):
    Q = os.path.join(os.path.expanduser("~"), "hermes-agent")
sys.path.insert(0, Q)
SRC = os.path.join(Q, "hermes_cli", "update_cmd_windows.py")


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
    name = "hermes_cli.update_cmd_windows"
    sp = importlib.util.spec_from_file_location(name, SRC)
    live = importlib.util.module_from_spec(sp)
    sys.modules[name] = live
    sp.loader.exec_module(live)
    return live


live = _load_live()

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
    if func == "marker":
        py = str(live._planned_stop_marker_path(Path(r.get("path", ""))))
    elif func == "prefix":
        py = live._lower_dir_prefix(Path(r.get("path", "")))
    elif func == "pids":
        profiles = {k: int(v) for k, v in (r.get("profiles") or {}).items()}
        unmapped = [dict(e) for e in (r.get("unmapped") or [])]
        alive = {int(x) for x in (r.get("alive") or [])}
        py = live._pending_relaunch_pids(profiles, unmapped, lambda pid: pid in alive)
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)
print("ORACLE w17_updatecmd_736da6: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
