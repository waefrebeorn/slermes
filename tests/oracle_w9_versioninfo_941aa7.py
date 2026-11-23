#!/usr/bin/env python3
"""Differential oracle for hermes_cli/version_info.py (gate w9_versioninfo_941aa7).

The git/stamp resolution dependencies are stubbed at import time; the LIVE
pure functions (_derived_version, _parse_nonnegative, VersionInfo.display_version)
are then called directly.
"""
import importlib.util
import json
import re
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
_st = types.ModuleType("hermes_cli.steward")
_st.UPDATE_MECHANISMS = ("build", "commit-build", "ci", "docker", "fallback",
                         "git", "local", "nix")
sys.modules["hermes_cli.steward"] = _st
_uc = types.ModuleType("hermes_cli.update_channel")
_uc.STABLE_TAG_RE = re.compile(r"v\d+\.\d+\.\d+")
sys.modules["hermes_cli.update_channel"] = _uc
_sf = types.ModuleType("hermes_cli._startup_fast")
_sf.project_root_str = lambda: QUARRY
sys.modules["hermes_cli._startup_fast"] = _sf


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


live = load("w9_versioninfo_941aa7_live", QUARRY + "/hermes_cli/version_info.py")


def dist_or_none(r):
    v = r.get("distance", "")
    return int(v) if v else None


def compute(r):
    f = r.get("func")
    if f == "derived_version":
        return live._derived_version(
            r.get("base", ""), dist_or_none(r), bool(r.get("dirty", False)),
            r.get("short_commit") or None)
    if f == "parse_nonnegative":
        return live._parse_nonnegative(r.get("value", ""))
    if f == "display_version":
        info = live.VersionInfo(
            base_version=r.get("base", ""),
            derived_version=r.get("derived", ""),
            distance=dist_or_none(r),
            commit=None, branch=None, source="unknown")
        return info.display_version
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
        print("ORACLE w9_versioninfo_941aa7: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w9_versioninfo_941aa7 %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w9_versioninfo_941aa7: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
