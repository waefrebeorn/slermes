#!/usr/bin/env python3
"""Differential oracle for pm/_uv.py (w5_uv_f7190a).

All IO/registry/sealed seams are stubbed; the LIVE _toolchain decision tree is
called and its outcome is compared to the C port.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

CTX = {}


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


pm = _mod("pm")
pm.__path__ = []


class InstallError(Exception):
    def __init__(self, name, reason, hint):
        super().__init__(f"{name}: {reason}")
        self.name, self.reason = name, reason


class FakeBinary:
    def __init__(self, path, exists):
        self.path, self._exists = path, exists

    def is_file(self):
        return self._exists

    def __fspath__(self):
        return self.path


class FakePackage:
    def __init__(self, name):
        self.name = name

    def binary(self, entry, target):
        return FakeBinary(CTX["paths"][self.name], CTX["binaries"][self.name])


class FakeStore:
    def entry(self, e):
        return e


def _facts():
    return {"uv": {"entry": "u"}, "python": {"entry": "p"}}


def installed_location(package, lockfile, target):
    return (_facts(), FakeStore()) if CTX["locations"][package.name] else None


def sealed():
    return CTX["sealed"]


def ensure(*a, **k):
    pass


def writable_copy(name, realize=False, explicit=False, why=""):
    CTX["writable_called"] = True
    return (_facts(), FakeStore()) if CTX["writable"] else None


_mod("pm.filesystem", native=lambda p: p)
_mod("pm.package", InstallError=InstallError)
_mod("pm.registry", get_package=lambda name: FakePackage(name))
_mod("pm.store", current_target=lambda: CTX["target"])
_mod("pm.install", _installed_location=installed_location, _lockfile=lambda: object(),
     ensure=ensure, sealed=sealed)
_mod("pm.user_facing", writable_copy=writable_copy)

SRC = Q + "/pm/_uv.py"
spec = importlib.util.spec_from_file_location("pmuv_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["pmuv_live"] = live
spec.loader.exec_module(live)

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
    CTX.update(
        target=r.get("target", ""), sealed=bool(r.get("sealed")),
        locations={"uv": bool(r.get("uv_loc")), "python": bool(r.get("py_loc"))},
        binaries={"uv": bool(r.get("uv_bin")), "python": bool(r.get("py_bin"))},
        paths={"uv": r.get("uv_path", "uvtool"), "python": r.get("py_path", "pytool")},
        writable=bool(r.get("writable")), writable_called=False,
    )

    if func == "toolchain":
        try:
            out = live._toolchain(realize=bool(r.get("realize", True)))
        except live.InstallError as exc:
            py = {"status": "error", "name": exc.name,
                  "kind": "unavailable" if "unavailable" in exc.reason else "missing"}
        else:
            py = None if out is None else {"status": "ok", "uv": str(out[0]), "python": str(out[1])}
    elif func == "needs_copy":
        try:
            live._toolchain(realize=True)
        except Exception:
            pass
        py = bool(CTX["writable_called"])
        got = bool(got)
    else:
        continue

    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w5_uv_f7190a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
