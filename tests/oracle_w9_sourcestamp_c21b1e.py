#!/usr/bin/env python3
"""Differential oracle for hermes_cli/source_stamp.py (gate w9_sourcestamp_c21b1e).

The git walk is replaced by a fixture-built VersionInfo, the clock by a frozen
datetime, and the receipt refresh by a no-op; write_source_stamp() itself is
the LIVE function under test.
"""
import importlib.util
import json
import sys
import tempfile
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# Parent-package bootstrap + a stubbed version_info: source_stamp only needs
# _git_version_info and _reset_version_info_cache from it.
if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg
_vi = types.ModuleType("hermes_cli.version_info")


class VersionInfo:
    def __init__(self, **kw):
        self.__dict__.update(kw)


_vi.VersionInfo = VersionInfo
_vi._reset_version_info_cache = lambda: None
_vi._git_version_info = lambda *a, **k: None
sys.modules["hermes_cli.version_info"] = _vi


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


live = load("w9_sourcestamp_c21b1e_live", QUARRY + "/hermes_cli/source_stamp.py")


class _Frozen:
    def __init__(self, s):
        self.s = s

    def isoformat(self):
        return self.s

    def strftime(self, fmt):
        return self.s


class _FrozenDatetime:
    current = ""

    @classmethod
    def now(cls, tz=None):
        return _Frozen(cls.current)


_ROOT = tempfile.mkdtemp(prefix="noc_w9_sourcestamp_")


def as_int(r, key):
    v = r.get(key, "")
    return int(v) if v else None


def compute(r):
    func = r.get("func")
    if func == "bootstrap_receipt_key":
        return live.BOOTSTRAP_RECEIPT
    if func != "write_source_stamp":
        return None
    info = VersionInfo(
        base_version=r.get("base_version", ""),
        derived_version=r.get("derived_version", ""),
        distance=as_int(r, "distance"),
        commit=r.get("commit") or None,
        branch=r.get("branch") or None,
        source="git",
        dirty=bool(r.get("dirty", False)),
        commit_date=as_int(r, "commit_date"),
        distribution=None,
    )
    live._git_version_info = lambda root, include_untracked=False: info
    live.refresh_bootstrap_receipt = lambda root, stamp: None
    live._reset_version_info_cache = lambda: None
    _FrozenDatetime.current = r.get("built_at", "")
    live.datetime = _FrozenDatetime
    return live.write_source_stamp(_ROOT, adopted=bool(r.get("adopted", False)))


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
        print("ORACLE w9_sourcestamp_c21b1e: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w9_sourcestamp_c21b1e %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w9_sourcestamp_c21b1e: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
