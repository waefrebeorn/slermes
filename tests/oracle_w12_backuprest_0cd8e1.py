#!/usr/bin/env python3
"""Differential oracle for hermes_cli/backup_restore.py (w12_backuprest_0cd8e1).

The live module imports a heavy chain at module scope; every one of those
imports is stubbed (parent packages bootstrapped) so only the pure leaf
functions execute. zipfile.namelist() and Path.name/resolve() are projected
through FakeZip / FakePath.
"""
import copy
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def mod(name, **attrs):
    if "." in name:
        parent, _ = name.rsplit(".", 1)
        pm = sys.modules.get(parent) or mod(parent)
        if not hasattr(pm, "__path__"):
            pm.__path__ = []
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class AdminUnsupported(Exception):
    pass


mod("hermes_cli")
mod("hermes_cli.stale_modules", drop_stale_root_modules=lambda: None)
mod("persistence")
mod("persistence.core")
mod("persistence.core.admin", AdminUnsupported=AdminUnsupported)
mod("persistence_assembly", open_store_admin=lambda *a, **k: None)
mod("sqlite_support", read_only_db_uri=lambda p: p)
mod("hermes_cli.config_backend", drop_other_access=lambda p: None)
mod("utils", atomic_replace=lambda a, b: b, mkstemp_beside=lambda *a, **k: (0, "x"),
    preserve_file_mode=lambda p: None, preserve_file_owner=lambda p: None,
    restore_file_mode=lambda *a: None, restore_file_owner=lambda *a: None)

SRC = Q + "/hermes_cli/backup_restore.py"
spec = importlib.util.spec_from_file_location("backup_restore_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["backup_restore_live"] = live
spec.loader.exec_module(live)


class FakeZip:
    def __init__(self, names):
        self._names = list(names or [])

    def namelist(self):
        return list(self._names)


class FakePath:
    def __init__(self, name, resolved):
        self._name = name
        self._resolved = resolved

    @property
    def name(self):
        return self._name

    def resolve(self):
        return types.SimpleNamespace(name=self._resolved)


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
    try:
        if func == "validate":
            ok, reason = live._validate_backup_zip(FakeZip(r.get("names")))
            py = {"ok": ok, "reason": reason}
        elif func == "prefix":
            py = live._detect_prefix(FakeZip(r.get("names")))
        elif func == "get":
            py = live._get_config_path_value(r.get("data"), tuple(r.get("dotted") or []))
        elif func == "set":
            data = copy.deepcopy(r.get("data"))
            live._set_config_path_value(data, tuple(r.get("dotted") or []), r.get("value"))
            py = data
        elif func == "is_store":
            py = live._is_store_file(FakePath(r.get("name", ""), r.get("resolved", "")))
        elif func == "names":
            py = sorted(live._STORE_FILE_NAMES)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w12_backuprest_0cd8e1: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
