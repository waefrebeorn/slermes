#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_windows_legacy.py (gate
w23_gatewaywin_830338).

The module is a bare-name cleanup pass over Windows autostart objects. Every
fact it consults is an OS seam, so the world a fixture describes is scripted:

  * ``hermes_cli.gateway_windows`` (the facade binding the module reaches
    through ``_w()``) is a namespace answering ``_TASK_NAME_DEFAULT``,
    ``get_task_name()``, ``_hermes_home()``, ``_startup_dir()``,
    ``_normalize_windows_path()``, ``_exec_schtasks()`` and
    ``_is_access_denied()``;
  * ``pathlib.Path`` (imported into the module's globals) is a FakePath over a
    virtual file system, so ``read_text(encoding="utf-8-sig")``, ``exists()``
    and ``unlink()`` are the fixture's: a read can raise OSError, an unlink can
    raise OSError with a recorded message.

Everything else -- which bare names belong to this home, the artifact order,
the task probe, the exact status/removal lines, the return values -- is the
live function.
"""
import contextlib
import importlib.util
import io
import json
import sys
from pathlib import Path as RealPath

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = RealPath(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "gateway_windows_legacy.py"
GATE = "w23_gatewaywin_830338"
BARE = "Hermes_Gateway"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("gateway_windows_legacy_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["gateway_windows_legacy_live"] = live
_spec.loader.exec_module(live)

# The real facade, for the two pure helpers the module borrows from it: the
# path normalizer is the module's whole ownership test, so it is compared
# against `hermes_cli.gateway_windows` itself rather than a copy.
try:
    from hermes_cli import gateway_windows as real_facade
except Exception:  # noqa: BLE001
    real_facade = None

WANTED = {"normalize_path", "join_path", "targets_this_home", "artifacts", "warn", "remove"}


def norm(value):
    """`_normalize_windows_path`: strip, no leading quote, "/" separators, no
    trailing separator, casefolded."""
    return str(value).strip().lstrip('"').replace("\\", "/").rstrip("/").casefold()


class FakeFs:
    """The fixture's files: original path -> text."""

    def __init__(self, files, unreadable=(), unlink_fail=None):
        files = files if isinstance(files, dict) else {}
        unreadable = unreadable if isinstance(unreadable, list) else []
        unlink_fail = unlink_fail if isinstance(unlink_fail, dict) else {}
        self.files = {str(k): str(v) for k, v in files.items()}
        self.unreadable = [norm(p) for p in (unreadable or [])]
        self.unlink_fail = {norm(k): str(v) for k, v in (unlink_fail or {}).items()}

    def key(self, path):
        want = norm(path)
        for k in self.files:
            if norm(k) == want:
                return k
        return None

    def read(self, path):
        k = self.key(path)
        if k is None:
            raise FileNotFoundError(2, "No such file or directory", str(path))
        if norm(path) in self.unreadable:
            raise OSError("unreadable")
        text = self.files[k]
        return text[1:] if text.startswith("\ufeff") else text

    def exists(self, path):
        return self.key(path) is not None

    def unlink(self, path):
        k = self.key(path)
        if norm(path) in self.unlink_fail:
            raise OSError(self.unlink_fail[norm(path)])
        if k is None:
            raise FileNotFoundError(2, "No such file or directory", str(path))
        del self.files[k]

    def remaining(self):
        return sorted(self.files)


FS = None


class FakePath:
    """`Path` over FakeFs; `Path / part` joins with one "/"."""

    def __init__(self, value=""):
        self._s = str(value)

    def __truediv__(self, part):
        base = self._s.rstrip("/")
        return FakePath(part if not base else base + "/" + str(part))

    def __str__(self):
        return self._s

    def __repr__(self):
        return "FakePath(%r)" % self._s

    def read_text(self, encoding="utf-8", errors="strict"):
        return FS.read(self._s)

    def exists(self):
        return FS.exists(self._s)

    def unlink(self):
        return FS.unlink(self._s)


class FakeW:
    """The `hermes_cli.gateway_windows` facade the module binds through `_w()`."""

    _TASK_NAME_DEFAULT = BARE

    def __init__(self, world):
        self.world = world

    def get_task_name(self):
        value = self.world.get("task_name")
        return BARE if value is None else value

    def _hermes_home(self):
        return FakePath(self.world.get("hermes_home") or "")

    def _startup_dir(self):
        return FakePath(self.world.get("startup_dir") or "")

    def _normalize_windows_path(self, value):
        return norm(value)

    def _exec_schtasks(self, args, timeout=None):
        key = "schtasks_delete" if args and args[0] == "/Delete" else "schtasks"
        answer = self.world.get(key) or self.world.get("schtasks") or {}
        return (int(answer.get("code", 1)), str(answer.get("stdout", "")),
                str(answer.get("stderr", "")))

    def _is_access_denied(self, detail):
        return bool(self.world.get("access_denied", False))


@contextlib.contextmanager
def patched(world):
    global FS
    FS = FakeFs(world.get("files"), world.get("unreadable"), world.get("unlink_fail"))
    w = FakeW(world)
    saved_w, saved_path = live._w, live.Path
    live._w = lambda: w
    live.Path = FakePath
    try:
        yield w, FS
    finally:
        live._w = saved_w
        live.Path = saved_path


def world_of(r):
    world = r.get("world")
    return world if isinstance(world, dict) else {}


def artifacts(world):
    with patched(world):
        found = live.legacy_launcher_artifacts()
    return [[kind, label, str(target)] for kind, label, target in found]


def warn(world):
    buf = io.StringIO()
    with patched(world):
        with contextlib.redirect_stdout(buf):
            result = live.warn_legacy_launchers()
    return {"result": bool(result), "lines": buf.getvalue().splitlines()}


def remove(world):
    buf = io.StringIO()
    with patched(world):
        with contextlib.redirect_stdout(buf):
            live.remove_legacy_launchers()
        left = FS.remaining()
    return {"lines": buf.getvalue().splitlines(), "remaining": left}


def expected(r):
    func = r.get("func")
    if func == "normalize_path":
        if real_facade is None:
            raise AssertionError("real gateway_windows facade unavailable")
        return real_facade._normalize_windows_path(str(r.get("value") or ""))
    if func == "join_path":
        with patched({}):
            return str(FakePath(str(r.get("base") or "")) / str(r.get("part") or ""))
    if func == "targets_this_home":
        world = {"hermes_home": r.get("hermes_home") or ""}
        with patched(world):
            return bool(live._targets_this_home(str(r.get("text") or "")))
    if func == "artifacts":
        return artifacts(world_of(r))
    if func == "warn":
        return warn(world_of(r))
    if func == "remove":
        return remove(world_of(r))
    raise AssertionError("unknown func %r" % (func,))


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
