#!/usr/bin/env python3
"""Differential oracle for hermes_platform/resolver/core.py (gate w23_core_082b31).

``locate_command`` is a passive probe: it reads file metadata and never opens,
spawns or connects.  The facts it would read are the fixture's world, so the
ambient process state is injected:

  * ``os`` is a namespace carrying name/sep/altsep/pathsep, the env mapping
    (PATHEXT, HOME/USERPROFILE), ``path`` (join/isabs/isfile/expandvars/
    expanduser) and ``access``;
  * ``shutil`` is a namespace whose ``which`` answers from the fixture's probe
    map, keyed by the executable name the caller hands it.

Everything else -- the explicit-path branch, the PATH-then-known_dirs candidate
order, one candidate per dir, the kind ladder and the command/source/present/
found views -- is the live code.  ``effective_path`` and the ``Resolution``
properties run live with no seam at all.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_platform" / "resolver" / "core.py"
GATE = "w23_core_082b31"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

for name, sub in (("hermes_platform", "hermes_platform"),
                  ("hermes_platform.resolver", "hermes_platform/resolver")):
    mod = types.ModuleType(name)
    mod.__path__ = [str(QUARRY / sub)]
    sys.modules.setdefault(name, mod)

_spec = importlib.util.spec_from_file_location("hermes_platform.resolver.core", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["hermes_platform.resolver.core"] = live
_spec.loader.exec_module(live)

WANTED = {"effective_path", "resolution", "locate"}


# ── the ambient seams, mirroring the harness's documented rules ──────────────

def _is_abs(path, os_name):
    if not path:
        return False
    if os_name == "nt":
        if path[0] in "/\\":
            return True
        return len(path) > 2 and path[0].isalpha() and path[1] == ":" and path[2] in "/\\"
    return path[0] == "/"


def _join(base, name, os_name, sep):
    base = base or ""
    name = name or ""
    if _is_abs(name, os_name):
        return name
    if not base:
        return name
    if base[-1] in "/\\":
        return base + name
    return base + sep + name


def _expand_vars(text, env):
    out = []
    i, n = 0, len(text)
    while i < n:
        if text[i] != "$":
            out.append(text[i])
            i += 1
            continue
        j = i + 1
        braced = False
        if j < n and text[j] == "{":
            braced = True
            j += 1
        k = j
        while k < n and (text[k].isalnum() or text[k] == "_") and (k - j) < 127:
            k += 1
        name = text[j:k]
        if braced:
            if k < n and text[k] == "}":
                k += 1
            else:
                out.append("$")
                i += 1
                continue
        if not name:
            out.append("$")
            i += 1
            continue
        out.append(env.get(name, text[i:k]))
        i = k
    return "".join(out)


def _expand_user(text, env, os_name):
    if not text or text[0] != "~":
        return text
    if len(text) > 1 and text[1] not in "/\\":
        return text
    home = env.get("USERPROFILE") if os_name == "nt" else env.get("HOME")
    if home is None:
        home = env.get("HOME") if os_name == "nt" else env.get("USERPROFILE")
    if home is None:
        return text
    return home + text[1:]


class FakePath:
    def __init__(self, os_name, sep, env, files):
        self._os_name = os_name
        self._sep = sep
        self._env = env
        self._files = files

    def join(self, base, name):
        return _join(base, name, self._os_name, self._sep)

    def isabs(self, path):
        return _is_abs(path, self._os_name)

    def isfile(self, path):
        return path in self._files

    def expandvars(self, text):
        return _expand_vars(text, self._env)

    def expanduser(self, text):
        return _expand_user(text, self._env, self._os_name)


def _world(r):
    os_name = r.get("os_name", "nt")
    sep = r.get("sep", "\\" if os_name == "nt" else "/")
    altsep = r.get("altsep", "/" if os_name == "nt" else None)
    pathsep = r.get("pathsep", ";" if os_name == "nt" else ":")
    env = dict(r.get("env") or {})
    files = set(r.get("files") or [])
    xok = set(r.get("xok") or [])
    which_map = dict(r.get("which") or {})
    fake_os = types.SimpleNamespace(
        name=os_name, sep=sep, altsep=altsep, pathsep=pathsep, environ=env,
        path=FakePath(os_name, sep, env, files),
        F_OK=0, X_OK=1,
        access=lambda path, mode: path in xok,
    )
    fake_shutil = types.SimpleNamespace(which=lambda name, path=None: which_map.get(name) or None)
    return fake_os, fake_shutil


def project(res):
    def row(c):
        return {"value": c.value, "source": c.source, "present": c.present}
    return {
        "kind": res.kind,
        "candidates": [row(c) for c in res.candidates],
        "command": list(res.command),
        "source": res.source,
        "present": [row(c) for c in res.present],
        "found": res.found,
    }


def effective_path(ctx):
    ctx = ctx if isinstance(ctx, dict) else {}
    if "path" not in ctx:
        return live.LookupContext().effective_path()
    return live.LookupContext(path=ctx["path"]).effective_path()


def resolution(r):
    kind = r.get("kind", "missing")
    cands = tuple(live.Candidate(value=c.get("value"), source=c.get("source"),
                                 present=bool(c.get("present")))
                  for c in (r.get("candidates") or []))
    return project(live.Resolution(kind, cands))


def locate(r):
    fake_os, fake_shutil = _world(r)
    ctx_raw = r.get("ctx") if isinstance(r.get("ctx"), dict) else {}
    kwargs = {}
    if "path" in ctx_raw:
        kwargs["path"] = ctx_raw["path"]
    if "pathext" in ctx_raw:
        kwargs["pathext"] = ctx_raw["pathext"]
    ctx = live.LookupContext(**kwargs)
    known = tuple(r.get("known_dirs") or ())
    saved_os, saved_shutil = live.os, live.shutil
    live.os, live.shutil = fake_os, fake_shutil
    try:
        return project(live.locate_command(r.get("name", ""), ctx, known_dirs=known))
    finally:
        live.os, live.shutil = saved_os, saved_shutil


def expected(r):
    f = r.get("func")
    if f == "effective_path":
        return effective_path(r.get("ctx"))
    if f == "resolution":
        return resolution(r)
    if f == "locate":
        return locate(r)
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
