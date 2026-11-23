#!/usr/bin/env python3
"""Differential oracle for hermes_constants_scratch.py (gate w5_hermesconst_d195ca).

lstat/scandir, permission classification and the birth stamp are injected per
case. The live _under/_same_birth/_scan_subtree/subtree_touch_state compute the
expected values, so the C port is compared against the real walk logic.
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


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


stuck = _mod("hermes_constants_scratch_stuck",
             is_permission_error=lambda exc: isinstance(exc, PermissionError),
             load_skips=lambda root: set(), STUCK_STATE_NAME=".stuck", STUCK_LOCK_NAME=".stuck.lock",
             tracking_supported=lambda: False, record=lambda *a, **k: None,
             rmtree_collecting_permission_errors=lambda entry: [])

spec = importlib.util.spec_from_file_location("w5_hermesconst_live", Q + "/hermes_constants_scratch.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w5_hermesconst_live"] = live
spec.loader.exec_module(live)


class _FPath:
    def __init__(self, s, is_dir, is_symlink):
        self.s = s
        self._is_dir = is_dir
        self._is_symlink = is_symlink
    def is_dir(self):
        return self._is_dir
    def is_symlink(self):
        return self._is_symlink
    def __str__(self):
        return self.s


class _Child:
    def __init__(self, d):
        self.d = d
    @property
    def path(self):
        return self.d.get("path", "")
    def stat(self, follow_symlinks=False):
        if not self.d.get("stat_ok", True):
            if self.d.get("perm_err", False):
                raise PermissionError("denied")
            raise OSError("stat failed")
        return types.SimpleNamespace(st_mtime=self.d.get("mtime", 0.0),
                                     st_nlink=self.d.get("nlink", 1))
    def is_dir(self, follow_symlinks=False):
        return bool(self.d.get("is_dir", False))
    def is_file(self, follow_symlinks=False):
        return bool(self.d.get("is_file", False))


class _Ctx:
    def __init__(self, children):
        self.children = children
    def __enter__(self):
        return self.children
    def __exit__(self, *exc):
        return False


def _install_fs(r):
    entries = r.get("entries") or []
    root_ok = r.get("root_stat_ok", True)
    root_mtime = r.get("root_mtime", 0.0)

    def fake_lstat(p):
        if not root_ok:
            raise OSError("lstat failed")
        return types.SimpleNamespace(st_mtime=root_mtime)

    def fake_scandir(current):
        for d in entries:
            if d.get("dir") == current and d.get("scandir_fail"):
                if d.get("perm_err", False):
                    raise PermissionError("scandir denied")
                raise OSError("scandir failed")
        return _Ctx([_Child(d) for d in entries
                     if d.get("dir") == current and not d.get("scandir_fail")])

    live.os = types.SimpleNamespace(lstat=fake_lstat, scandir=fake_scandir)


def _fake_path(r):
    return _FPath(r.get("path", ""), r.get("root_is_dir", True), r.get("root_is_symlink", False))


def compute(r):
    f = r.get("func")
    if f == "under":
        live.os = types.SimpleNamespace(sep=r.get("sep", "/"))
        return live._under(r.get("path", ""), r.get("root", ""))
    if f == "same_birth":
        live._HAS_PROC = bool(r.get("has_proc", False))
        live._birth_stamp = lambda proc: r.get("now", 0.0)
        return live._same_birth(object(), r.get("started", 0.0))
    if f in ("scan_subtree", "subtree_touch_state"):
        _install_fs(r)
        live.stuck.is_permission_error = lambda exc: isinstance(exc, PermissionError)
        path = _fake_path(r)
        cutoff = r.get("cutoff", 0.0)
        if f == "scan_subtree":
            state, denied = live._scan_subtree(path, cutoff)
            return {"state": state, "denied_dir": denied}
        return live.subtree_touch_state(path, cutoff)
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
        print("MISMATCH hermesconst %s args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)

print("ORACLE w5_hermesconst_d195ca: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
