#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_cmd_zip.py (gate w12_updatecmd_8df38e)."""
import importlib.util
import json
import os
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m

_er = types.ModuleType("hermes_cli._early_recovery")
_er._keep_aside = lambda p: p
sys.modules["hermes_cli._early_recovery"] = _er
_ez = types.ModuleType("hermes_cli._early_recovery_zip")
_ez.ZIP_SWAP_JOURNAL = "j"
_ez.restore_interrupted_zip_swap = lambda *a: None
_ez.write_zip_swap_journal = lambda *a: None
_ez.zip_entry_identity = lambda *a, **k: "x"
_ez.zip_swap_owner_lock = lambda *a, **k: None
sys.modules["hermes_cli._early_recovery_zip"] = _ez
_cc = types.ModuleType("hermes_cli.update_cmd_common")
_cc._record_stop = lambda *a, **k: None
sys.modules["hermes_cli.update_cmd_common"] = _cc

live = load("w12_updatecmd_8df38e_live", QUARRY + "/hermes_cli/update_cmd_zip.py")

def compute(r):
    f = r.get("func")
    if f == "status_top_level":
        return live._status_top_level(r.get("path", ""))
    if f == "is_zip_preserved_entry_status_line":
        return live._is_zip_preserved_entry_status_line(r.get("line", ""), r.get("shipped"))
    if f == "is_zip_staging_artifact_status_line":
        return live._is_zip_staging_artifact_status_line(r.get("line", ""), r.get("staged", []))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print("ORACLE w12_updatecmd_8df38e: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w12_updatecmd_8df38e %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w12_updatecmd_8df38e: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
