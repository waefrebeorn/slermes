#!/usr/bin/env python3
"""Differential oracle for hermes_cli/backup_atomic.py (gate w10_backupatom_f4248f).

The live functions' filesystem collaborators (os / Path.rglob) are injected
seams; the C replay consumes the same seam inputs from the fixture.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/backup_atomic.py"
sys.path.insert(0, QUARRY)

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg

_bs = types.ModuleType("hermes_cli.backup_staging")
_bs.staged_backup_file = lambda *a, **k: None
sys.modules["hermes_cli.backup_staging"] = _bs
_ut = types.ModuleType("utils")
_ut.fsync_directory = lambda p: None
sys.modules["utils"] = _ut

spec = importlib.util.spec_from_file_location("w10_backupatom_f4248f_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w10_backupatom_f4248f_live"] = live
spec.loader.exec_module(live)

UNSUPPORTED = sorted(int(x) for x in live._DIR_FSYNC_UNSUPPORTED)


class FP:
    def __init__(self, tag, kind="other"):
        self.tag, self.kind = tag, kind

    def __str__(self):
        return self.tag

    def is_file(self):
        return self.kind == "file"

    def is_dir(self):
        return self.kind == "dir"

    def is_symlink(self):
        return self.kind == "symlink"


class Staging:
    def __init__(self, tag, entries):
        self.tag = tag
        self._entries = [FP(e.get("path", ""), e.get("kind", "other")) for e in entries]

    def __str__(self):
        return self.tag

    def rglob(self, pattern):
        return self._entries


class FakeFsyncOS:
    def __init__(self, name, err):
        self.name = name
        self.err = err
        self.O_RDONLY = 0
        self.raised = False

    def open(self, path, flags):
        return 3

    def fsync(self, fd):
        if self.err is not None:
            self.raised = True
            raise OSError(int(self.err), "probe")

    def close(self, fd):
        return None


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    func = r.get("func")
    name = r.get("os_name") or "posix"
    if func == "normalize_modes":
        rec = []
        fake = types.SimpleNamespace(
            name=name,
            chmod=lambda p, m: rec.append({"path": str(p), "mode": int(m)}))
        old = live.os
        live.os = fake
        try:
            live._normalize_private_modes(
                FP(r.get("root") or "", "dir"),
                Staging(r.get("staging") or "", r.get("entries") or []))
        finally:
            live.os = old
        return rec
    if func == "fsync_dir":
        err = r.get("err")
        fake = FakeFsyncOS(name, err)
        old = live.os
        live.os = fake
        try:
            try:
                live._fsync_staged_dir(FP("D"))
            except OSError:
                pass
        finally:
            live.os = old
        if name == "nt":
            return 0
        if not fake.raised:
            return 1
        return 2 if int(err) in UNSUPPORTED else 3
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except json.JSONDecodeError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w10_backupatom_f4248f: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH w10_backupatom_f4248f {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE w10_backupatom_f4248f: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
