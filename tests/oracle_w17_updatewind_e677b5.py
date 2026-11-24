#!/usr/bin/env python3
"""Oracle for the C port of hermes_cli/update_windows_acl.py (gate w17_updatewind_e677b5).

Loads the LIVE module via importlib.util.spec_from_file_location (parent-package
bootstrap + sys.modules registration), injects controlled hermes_dir_acl and
hermes_constants stubs, and drives the real repair_windows_home_acl() for each
fixture case. The harness seam returns the RuntimeError message (or JSON null
when the live function returns None); the oracle diffs the two. Exit 1 on any
mismatch.
"""
import importlib.util
import json
import logging
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
logging.disable(logging.CRITICAL)

Q = "C:/Users/eman5/hermes-agent"
if not os.path.isfile(os.path.join(Q, "hermes_cli", "update_windows_acl.py")):
    Q = os.path.join(os.path.expanduser("~"), "hermes-agent")
sys.path.insert(0, Q)
SRC = os.path.join(Q, "hermes_cli", "update_windows_acl.py")


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
    name = "hermes_cli.update_windows_acl"
    sp = importlib.util.spec_from_file_location(name, SRC)
    live = importlib.util.module_from_spec(sp)
    sys.modules[name] = live
    sp.loader.exec_module(live)
    return live


live = _load_live()


def live_outcome(r):
    home = r.get("home", "")
    granted = r.get("granted") or []
    skipped = r.get("skipped") or []
    failed = [tuple(pair) for pair in (r.get("failed") or [])]
    probe = bool(r.get("probe_elevated", False))
    e = r.get("elevated")  # None / True / False from JSON

    acl = types.ModuleType("hermes_dir_acl")
    acl.process_is_elevated = lambda: probe
    def grant(h, **kw):
        # Catch a wrong mental model of the live call site: ALL live args.
        if h != home or kw != {"pin_root": True, "elevated": True}:
            raise AssertionError("grant_tree_user_access(%r, %r)" % (h, kw))
        return granted, skipped, failed
    acl.grant_tree_user_access = grant
    sys.modules["hermes_dir_acl"] = acl

    constants = types.ModuleType("hermes_constants")
    constants.get_hermes_home = lambda: home
    sys.modules["hermes_constants"] = constants

    try:
        live.repair_windows_home_acl(elevated=e)
        return None
    except Exception as exc:
        return str(exc)


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") != "repair":
        continue
    cases += 1
    py = live_outcome(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w17_updatewind_e677b5: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
