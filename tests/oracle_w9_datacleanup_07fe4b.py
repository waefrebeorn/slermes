#!/usr/bin/env python3
"""Differential oracle for hermes_cli/data_cleanup.py (w9_datacleanup_07fe4b gate).

External path resolution and the filesystem are fixture seams. The live
plan_data_removal() runs unmodified against a fake pathlib.Path backed by the
same virtual tree the C port reads from the fixture.
"""
import importlib.util
import json
import posixpath
import stat
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

FS = {}
SPEC = {}
USER_HOME = "/home/user"


class FakePath:
    def __init__(self, value):
        s = str(value)
        self._s = posixpath.normpath(s) if s else "/"

    def __str__(self):
        return self._s

    def __repr__(self):
        return f"FakePath({self._s!r})"

    def __eq__(self, other):
        return isinstance(other, FakePath) and self._s == other._s

    def __hash__(self):
        return hash(self._s)

    def __lt__(self, other):
        return self._s < other._s

    def __truediv__(self, part):
        return FakePath(posixpath.join(self._s, str(part)))

    @property
    def anchor(self):
        return "/"

    @property
    def name(self):
        return posixpath.basename(self._s)

    @property
    def parent(self):
        return FakePath(posixpath.dirname(self._s) or "/")

    def resolve(self):
        e = FS.get(self._s)
        if e and e.get("resolved"):
            return FakePath(e["resolved"])
        return self

    def lstat(self):
        e = FS.get(self._s)
        if not e or not e.get("exists", True):
            raise FileNotFoundError(self._s)
        return types.SimpleNamespace(st_mode=stat.S_IFDIR if e.get("dir") else stat.S_IFREG)

    def is_symlink(self):
        e = FS.get(self._s)
        return bool(e and e.get("link"))

    def is_file(self):
        e = FS.get(self._s)
        return bool(e and e.get("exists", True) and not e.get("dir"))

    def is_dir(self):
        e = FS.get(self._s)
        return bool(e and e.get("exists", True) and e.get("dir"))

    def exists(self):
        e = FS.get(self._s)
        return bool(e and e.get("exists", True))

    def is_relative_to(self, other):
        other = str(other)
        return self._s == other or (other == "/" and self._s.startswith("/")) or self._s.startswith(other.rstrip("/") + "/")

    def iterdir(self):
        prefix = self._s.rstrip("/") + "/"
        kids = [p for p in FS if p.startswith(prefix) and posixpath.dirname(p) == self._s
                and FS[p].get("exists", True)]
        return [FakePath(p) for p in sorted(kids)]

    @staticmethod
    def home():
        return FakePath(USER_HOME)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _pkg(name):
    m = _mod(name)
    m.__path__ = []
    return m


_pkg("hermes_cli")
_pkg("pm")
_pkg("agent")
_pkg("agent.checkpoints")
_mod("pm.filesystem", is_junction=lambda p: bool((FS.get(str(p)) or {}).get("junction")))
_mod("pm.environments",
     base_venv=lambda p: FakePath(SPEC.get("base_venv", "/project/.venv")),
     store_root=lambda p: FakePath(SPEC.get("store_root", "/project/.store")),
     installs_root=lambda: FakePath(SPEC.get("installs_root", "/opt/hermes/installs")))


def _gdr(home=None):
    return FakePath(SPEC.get("machine", str(home)))


_mod("hermes_constants", get_default_hermes_root=_gdr)
_mod("hermes_cli.steward", is_bundled_payload=lambda p: bool(SPEC.get("bundle_app_root")))
_mod("hermes_cli.bundled_app",
     NotBundledApp=type("NotBundledApp", (Exception,), {}),
     resolve_bundle_layout=lambda p: types.SimpleNamespace(app_root=FakePath(SPEC["bundle_app_root"])))
_mod("agent.checkpoints.pruning", store_lock_path=lambda p: FakePath(SPEC.get("store_lock", "/store/.lock")))

# The live module captures `from pathlib import Path` at import time. Inject the
# fake only for that exec, then restore the real stdlib module for everything else.
real_pathlib = sys.modules.get("pathlib")
fake_pathlib = types.ModuleType("pathlib")
fake_pathlib.Path = FakePath
sys.modules["pathlib"] = fake_pathlib
SRC = Q + "/hermes_cli/data_cleanup.py"
spec = importlib.util.spec_from_file_location("w9dc_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w9dc_live"] = live
try:
    spec.loader.exec_module(live)
finally:
    if real_pathlib is not None:
        sys.modules["pathlib"] = real_pathlib
    else:
        del sys.modules["pathlib"]

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "plan":
        continue
    SPEC.clear()
    SPEC.update({k: v for k, v in r.items() if k not in ("func", "case", "out")})
    FS.clear()
    for e in SPEC.get("entries", []):
        FS[posixpath.normpath(e["path"])] = e
    if SPEC.get("canonical_present"):
        canonical = posixpath.join(posixpath.normpath(SPEC["home"]), "hermes-agent")
        FS[posixpath.join(canonical, "hermes_cli", "__init__.py")] = {"path": canonical}
    USER_HOME = SPEC.get("user_home", "/home/user")

    try:
        plan = live.plan_data_removal(
            FakePath(SPEC["home"]),
            FakePath(SPEC.get("project", "/project")),
            FakePath(SPEC["userdata"]) if SPEC.get("userdata") else None,
        )
        py = {"remove": [str(p) for p in plan.remove], "keep": [str(p) for p in plan.keep]}
    except Exception as exc:
        py = {"error": str(exc)}
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w9_datacleanup_07fe4b: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
