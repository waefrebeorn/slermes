#!/usr/bin/env python3
"""Differential oracle for pm/paths.py (w5_paths_f8daf3).

__file__-derived roots and the HERMES_INSTALL_ROOT env seam are injected;
path spelling is pinned to POSIX with a PurePosixPath stand-in so the live
functions are directly comparable to the C port.
"""
import importlib.util
import json
import sys
import types
from pathlib import PurePosixPath

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


class PPath(PurePosixPath):
    def resolve(self):
        return self


SRC = Q + "/pm/paths.py"
spec = importlib.util.spec_from_file_location("pmpaths_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["pmpaths_live"] = live
spec.loader.exec_module(live)
live.Path = PPath

hc = types.ModuleType("hermes_constants")
hc.get_default_hermes_root = lambda: PPath("HERMES_ROOT_PLACEHOLDER")
sys.modules["hermes_constants"] = hc

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    got = r.get("out")

    if func == "install_root":
        old = live.os.environ
        if "env" in r and r["env"] is not None:
            live.os.environ = {"HERMES_INSTALL_ROOT": r["env"]}
        else:
            live.os.environ = {}
        live.repo_root = lambda: PPath(r.get("repo", "repo"))
        try:
            py = str(live.install_root())
        finally:
            live.os.environ = old
    elif func == "install_stamp_path":
        live.repo_root = lambda: PPath(r.get("repo", "repo"))
        live.install_root = lambda: PPath(r.get("inst", "inst"))
        py = str(live.install_stamp_path(PPath(r.get("project", "repo"))))
    elif func == "partials_root":
        hc.get_default_hermes_root = lambda: PPath(r.get("root", "hermes"))
        py = str(live.partials_root())
    elif func == "facts_path":
        live.store_root = lambda: PPath(r.get("store", "store"))
        py = str(live.facts_path())
    else:
        continue

    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w5_paths_f8daf3: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
