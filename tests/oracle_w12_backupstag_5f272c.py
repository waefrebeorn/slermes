#!/usr/bin/env python3
"""Differential oracle for w12_backupstag_5f272c (hermes_cli/backup_staging.py).

Filesystem facts (lstat plain-ness, lease payload, directory entries) are
injected from the fixture through fake Path-like objects; the live module's
own regex/name/marker/entry logic is what actually runs.
"""
import importlib.util
import io
import json
import stat
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# Parent-package bootstrap: hermes_cli is registered before the child module is
# executed, so its `from hermes_cli.termination_guard import ...` resolves here.
hermes_cli = _mod("hermes_cli")
hermes_cli.__path__ = []
_mod("hermes_cli.termination_guard", unwind_on_termination=lambda: None)

SRC = Q + "/hermes_cli/backup_staging.py"
spec = importlib.util.spec_from_file_location("w12_backupstag_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w12_backupstag_live"] = live
spec.loader.exec_module(live)


class _Ent:
    def __init__(self, name, plain=True, directory=False, data=b""):
        self.name = name
        self._plain = plain
        self._directory = directory
        self._data = data

    def lstat(self):
        mode = (stat.S_IFDIR if self._directory else stat.S_IFREG) | 0o700
        ns = types.SimpleNamespace(st_mode=mode, st_nlink=1)
        if not self._plain:
            ns.st_file_attributes = 0x400  # FILE_ATTRIBUTE_REPARSE_POINT
        return ns

    def open(self, mode):
        assert mode == "rb"
        return io.BytesIO(self._data)


class _Stage:
    def __init__(self, spec):
        self.name = spec.get("name", "")
        self._plain = bool(spec.get("stage_plain", True))
        lease = (spec.get("lease", "") or "").encode("ascii")
        self._lease = _Ent(live._LEASE, bool(spec.get("lease_plain", True)),
                           data=b" " + lease)  # reserved first byte, then seek(1)
        self._entries = [_Ent(e.get("name", ""), bool(e.get("plain", True)))
                         for e in spec.get("entries", [])]

    def __truediv__(self, other):
        if other == live._LEASE:
            return self._lease
        raise KeyError(other)

    def iterdir(self):
        return list(self._entries)

    def lstat(self):
        return _Ent(self.name, self._plain, directory=True).lstat()


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
        if func == "marker":
            py = live._marker(types.SimpleNamespace(name=r.get("name", ""))).decode("ascii")
        elif func == "is_backup_name":
            py = bool(live._NAME.fullmatch(r.get("name", "")))
        elif func == "is_legacy_name":
            py = bool(live._LEGACY_NAME.fullmatch(r.get("name", "")))
        elif func == "stage_entries":
            v = live._stage_entries(_Stage({"entries": r.get("entries", [])}))
            py = [p.name for p in v] if v is not None else None
        elif func == "is_backup_staging":
            py = bool(live.is_backup_staging(_Stage(r)))
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
print(f"ORACLE w12_backupstag_5f272c: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
