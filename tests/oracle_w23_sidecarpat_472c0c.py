#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/photon/sidecar_paths.py
(gate w23_sidecarpat_472c0c).

``resolve_sidecar_dir`` is a filesystem decision ladder: a probe write, a
node_modules existence check, a lockfile-vs-install-marker mtime compare, a
mirror mkdir/copy pass and an OSError fallback.  Those are the OS seams, so the
world the fixture describes is scripted:

  * ``Path`` becomes a subclass whose ``exists``/``mkdir``/``touch``/``unlink``
    answer the fixture's facts (writability probe, node_modules, mirror files);
  * ``os`` is a namespace supplying only ``getenv`` for ``PHOTON_SIDECAR_DIR``;
  * ``shutil.copy2`` / ``filecmp.cmp`` record and predict the mirror refresh;
  * ``hermes_constants.get_hermes_home`` answers the fixture's home.

Everything else -- the branch order, the early returns, the copy filter and the
exact path text ``Path`` renders -- is the live function.  ``dir_writable``,
``_sidecar_dir``, ``_npm_error_log`` and the two module constants run live too.
"""
import contextlib
import importlib.util
import json
import pathlib
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "photon" / "sidecar_paths.py"
GATE = "w23_sidecarpat_472c0c"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("sidecar_paths_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["sidecar_paths_live"] = live
_spec.loader.exec_module(live)

WANTED = {"resolve", "sidecar_dir", "npm_error_log", "writable", "mirror_files",
          "source_default", "path_norm"}

WPath = type(pathlib.Path())


class FakePath(WPath):
    """A Path whose filesystem answers come from the fixture."""

    exists_set = frozenset()
    mkdir_fail = False
    touch_ok = True

    def exists(self, *args, **kwargs):  # noqa: ARG002
        return str(self) in FakePath.exists_set

    def mkdir(self, parents=False, exist_ok=True):  # noqa: ARG002
        if FakePath.mkdir_fail:
            raise OSError("read-only filesystem")
        return None

    def touch(self, *args, **kwargs):  # noqa: ARG002
        if not FakePath.touch_ok:
            raise OSError("permission denied")
        return None

    def unlink(self, *args, **kwargs):  # noqa: ARG002
        if not FakePath.touch_ok:
            raise OSError("permission denied")
        return None


class Stubs(contextlib.AbstractContextManager):
    """Install the filesystem/env seams for one fixture case."""

    def __init__(self, world):
        self.world = world if isinstance(world, dict) else {}
        self.copies = []
        self.saved = None

    def _copy2(self, a, b):  # noqa: ARG002
        self.copies.append(str(a).replace("\\", "/").rsplit("/", 1)[-1])

    def __enter__(self):
        w = self.world
        src = w.get("source_dir")
        base = WPath(src) if src is not None else live.SOURCE_SIDECAR_DIR
        exists = set()
        FakePath.exists_set = exists
        FakePath.mkdir_fail = bool(w.get("mirror_fail"))
        if w.get("node_modules"):
            exists.add(str(base / "node_modules"))
        for name in w.get("src_files") or []:
            exists.add(str(base / name))
        mirror = WPath(w.get("hermes_home") or "") / "photon" / "sidecar"
        for name in w.get("dst_files") or []:
            exists.add(str(mirror / name))

        self.saved = (live.Path, live.os, live.shutil, live.filecmp,
                      live._dir_writable, live._lock_newer_than_install,
                      live._SIDECAR_DIR, live._NPM_ERROR_LOG)
        self.saved_hc = sys.modules.get("hermes_constants")

        override = w.get("override")
        live.Path = FakePath
        live.os = types.SimpleNamespace(
            getenv=lambda k, d=None: (override or None) if k == "PHOTON_SIDECAR_DIR" else d)
        live.shutil = types.SimpleNamespace(copy2=self._copy2)
        live.filecmp = types.SimpleNamespace(
            cmp=lambda a, b, shallow=False: bool(w.get("same")))  # noqa: ARG005
        consts = types.ModuleType("hermes_constants")
        consts.get_hermes_home = lambda: FakePath(w.get("hermes_home") or "")
        sys.modules["hermes_constants"] = consts
        live._dir_writable = lambda p: bool(w.get("writable"))  # noqa: ARG005
        live._lock_newer_than_install = lambda p: bool(w.get("lock_newer"))  # noqa: ARG005
        return self

    def __exit__(self, *exc):
        (live.Path, live.os, live.shutil, live.filecmp,
         live._dir_writable, live._lock_newer_than_install,
         live._SIDECAR_DIR, live._NPM_ERROR_LOG) = self.saved
        if self.saved_hc is None:
            sys.modules.pop("hermes_constants", None)
        else:
            sys.modules["hermes_constants"] = self.saved_hc
        return False


def resolve(world):
    world = world if isinstance(world, dict) else {}
    with Stubs(world) as st:
        out = live.resolve_sidecar_dir(world.get("source_dir"))
    return {"dir": str(out), "copies": st.copies}


def sidecar_dir(world):
    world = world if isinstance(world, dict) else {}
    with Stubs(world):
        live._SIDECAR_DIR = FakePath(world["cached"]) if world.get("cached") else None
        out = live._sidecar_dir()
    return str(out)


def npm_error_log(world):
    world = world if isinstance(world, dict) else {}
    with Stubs(world):
        live._NPM_ERROR_LOG = FakePath(world["log"]) if world.get("log") else None
        memo = world.get("sidecar_dir") or world.get("cached")
        live._SIDECAR_DIR = FakePath(memo) if memo else None
        out = live._npm_error_log()
    return str(out)


def writable(world):
    world = world if isinstance(world, dict) else {}
    FakePath.touch_ok = bool(world.get("touch_ok"))
    saved = live.Path
    live.Path = FakePath
    try:
        return bool(live.dir_writable(FakePath("probe")))
    finally:
        live.Path = saved


def path_norm(world):
    world = world if isinstance(world, dict) else {}
    return str(Path(world.get("path") or ""))


def expected(r):
    f = r.get("func")
    if f == "resolve":
        return resolve(r.get("world"))
    if f == "sidecar_dir":
        return sidecar_dir(r.get("world"))
    if f == "npm_error_log":
        return npm_error_log(r.get("world"))
    if f == "writable":
        return writable(r.get("world"))
    if f == "mirror_files":
        return list(live._MIRROR_FILES)
    if f == "source_default":
        return str(live.SOURCE_SIDECAR_DIR)
    if f == "path_norm":
        return path_norm(r.get("world"))
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
