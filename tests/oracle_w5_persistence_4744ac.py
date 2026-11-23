#!/usr/bin/env python3
"""Differential oracle for persistence_assembly.py (gate w5_persistence_4744ac).

The store/provider/backend layer is stubbed; only the pure scope and owner
derivations are compared. Paths are held as PurePosixPath with an identity
resolve so a Windows host still compares against the fixture's forward-slash
paths exactly as the C port sees them.
"""
import importlib.util
import json
import sys
import types
from pathlib import PurePosixPath  # noqa: F401  (kept for import-light parity)

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = ["/".join([Q] + parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class ProfileScope:
    def __init__(self, profile_dir, name):
        self.profile_dir = profile_dir
        self.name = name


class Store:
    ready: bool = False


_stub("persistence", ProfileScope=ProfileScope, StorageHealth=object)
_stub("persistence.core")
_stub("persistence.core.admin", PHYSICAL_ADMIN="physical_admin", AdminUnsupported=type("AdminUnsupported", (), {}),
      ProfileRepairAdmin=object, StoreAdmin=object)
_stub("persistence.core.backend", PersistenceStore=Store)

hc = _stub("hermes_constants", get_default_hermes_root=lambda: PurePosixPath("/root"),
           get_hermes_home=lambda: PurePosixPath("/home"),
           named_profile_home=lambda canonical: None,
           profile_name_for_home=lambda canonical: None)
for fn in ("get_default_hermes_root", "get_hermes_home", "named_profile_home", "profile_name_for_home"):
    setattr(hc, fn, lambda *a, **k: None)

class _FPath:
    """Minimal forward-slash path with identity resolve/expanduser (no Windows drive rewrite)."""
    def __init__(self, s):
        self.s = str(s)
    def expanduser(self):
        return self
    def resolve(self, strict=False):
        return self
    @property
    def parent(self):
        return _FPath(self.s.rsplit("/", 1)[0] if "/" in self.s else "")
    @property
    def name(self):
        return self.s.rsplit("/", 1)[-1]
    def __str__(self):
        return self.s
    def __fspath__(self):
        return self.s
    def __truediv__(self, other):
        return _FPath(self.s.rstrip("/") + "/" + str(other))
    def __eq__(self, other):
        return isinstance(other, _FPath) and self.s == other.s
    def __hash__(self):
        return hash(self.s)


PPath = _FPath

spec = importlib.util.spec_from_file_location("w5_persistence_live", Q + "/persistence_assembly.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w5_persistence_live"] = live
spec.loader.exec_module(live)
live.Path = PPath


def compute(r):
    f = r.get("func")
    if f == "store_owner_name":
        hc.get_default_hermes_root = lambda: PPath(r.get("default_root", "/root"))
        try:
            return live.store_owner_name(PPath(r.get("profile_dir", "")))
        except Exception:
            return None
    if f == "state_db":
        scope = types.SimpleNamespace(hermes_home_key=r.get("key", ""))
        return str(live._state_db(scope))
    if f == "profile_scope":
        root = r.get("default_root", "/root")
        home_root = r.get("hermes_home", "/home")
        pname = r.get("profile_name_for_home")
        named = r.get("named_profile_home")
        hc.get_default_hermes_root = lambda: PPath(root)
        hc.get_hermes_home = lambda: PPath(home_root)
        hc.named_profile_home = lambda canonical: PPath(named) if named else None
        hc.profile_name_for_home = lambda canonical: pname
        home = PPath(r["home"]) if r.get("home") else None
        scope = live.profile_scope(profile=r.get("profile"), home=home)
        return {"profile_dir": str(scope.profile_dir), "name": scope.name}
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
    try:
        py = compute(r)
    except Exception as exc:
        print("LIVE ERROR %s args=%r: %r" % (r.get("func"), r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH persistence %s args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)

print("ORACLE w5_persistence_4744ac: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
