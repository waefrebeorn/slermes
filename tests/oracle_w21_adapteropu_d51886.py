#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/discord/adapter_opus.py
(gate w21_adapteropu_d51886).

`_find_discord_windows_bundled_opus` and `load_opus_codec` are lifted out of the
LIVE source (AST, sync/async) and driven against the OS/library boundary the
module itself talks to:

  sys.platform / struct.calcsize("P")      -> the case's platform/ptr_bits
  os.path.isfile, Path(...).is_file()      -> membership in the case's `files`
  ctypes.util.find_library("opus")          -> the case's `find_library`
  discord.opus.is_loaded/load_opus          -> the case's load_script
  threading.Lock, logging                   -> real lock, recorded warnings

The probe-once guard (`_probed`) and the candidate ORDER (bundled, then
find_library, Homebrew only when find_library missed and only on darwin), the
per-candidate except, and the single "codec not found" warning are the live
module's own decisions and stay under test.
"""
import __future__
import ast
import json
import sys
import threading
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "discord" / "adapter_opus.py"
GATE = "w21_adapteropu_d51886"
_ANN = __future__.annotations.compiler_flag

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

FUNCS = ("_find_discord_windows_bundled_opus", "load_opus_codec")


class FakePath:
    """The seam for Path(...).resolve().parent / "bin" / name + is_file()."""

    def __init__(self, path):
        self._p = str(path)

    def resolve(self):
        return self

    @property
    def parent(self):
        p = self._p.replace("\\", "/")
        head = p.rsplit("/", 1)[0] if "/" in p else ""
        return FakePath(head)

    def __truediv__(self, other):
        return FakePath(self._p.rstrip("/") + "/" + str(other))

    def is_file(self):
        return self._p in CURRENT["files"]

    def __str__(self):
        return self._p

    def __fspath__(self):
        return self._p


CURRENT = {"files": set(), "loaded": False, "attempts": [], "warnings": []}


class RecLogger:
    def warning(self, msg, *args, **kwargs):
        CURRENT["warnings"].append((msg % args) if args else msg)

    def debug(self, *a, **k):
        pass

    def info(self, *a, **k):
        pass


def build_discord(case):
    """discord module stub whose opus object records/loads per load_script."""
    state = CURRENT

    class Opus:
        def is_loaded(self):
            state["touched"] = True
            return state["loaded"]

        def load_opus(self, path):
            state["attempts"].append(path)
            idx = len(state["attempts"]) - 1
            script = state["script"]
            outcome = script[idx] if idx < len(script) else "no"
            if outcome == "ok":
                state["loaded"] = True
            elif outcome == "fail":
                raise RuntimeError("cannot load %s" % path)
            state["outcomes"].append(outcome)

    opus = Opus()
    if case.get("no_opus_module"):
        return types.SimpleNamespace()
    if "opus_file" in case:
        opus.__file__ = case.get("opus_file")
    return types.SimpleNamespace(opus=opus)


def install_stubs(case):
    """sys/struct/os/ctypes/discord as the module sees them."""
    fps = case.get("platform") or "linux"
    ptr = case.get("ptr_bits") or 64
    fake_sys = types.SimpleNamespace(platform=fps)
    fake_struct = types.SimpleNamespace(calcsize=lambda fmt: max(1, ptr // 8))
    fake_os = types.SimpleNamespace(path=types.SimpleNamespace(
        isfile=lambda p: str(p) in CURRENT["files"]))
    fake_ctypes = types.ModuleType("ctypes")
    fake_util = types.ModuleType("ctypes.util")
    fake_util.find_library = lambda name: case.get("find_library")
    fake_ctypes.util = fake_util
    fake_discord = build_discord(case)
    mods = {"ctypes": fake_ctypes, "ctypes.util": fake_util, "discord": fake_discord}
    for name, mod in mods.items():
        sys.modules[name] = mod
    return {
        "sys": fake_sys,
        "struct": fake_struct,
        "os": fake_os,
        "Path": FakePath,
        "logger": RecLogger(),
        "threading": threading,
    }


NS = {}


def load_ns():
    NS.clear()
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in FUNCS:
            node = ast.parse(ast.unparse(node)).body[0]
            node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), NS)


def drive(case, func):
    CURRENT["files"] = set(case.get("files") or [])
    CURRENT["loaded"] = bool(case.get("already_loaded"))
    CURRENT["attempts"] = []
    CURRENT["warnings"] = []
    CURRENT["script"] = list(case.get("load_script") or [])
    CURRENT["outcomes"] = []
    CURRENT["touched"] = False
    for key, value in install_stubs(case).items():
        NS[key] = value
    NS["_probed"] = bool(case.get("probed"))
    NS["_probe_lock"] = threading.Lock()
    if func == "bundled":
        module = types.SimpleNamespace()
        if not case.get("no_opus_module"):
            module = types.SimpleNamespace(opus=types.SimpleNamespace(
                __file__=case.get("opus_file")))
        return NS["_find_discord_windows_bundled_opus"](module)
    NS["load_opus_codec"]()
    return {
        "attempts": list(CURRENT["attempts"]),
        "load_failures": sum(1 for w in CURRENT["warnings"] if "failed to load" in w),
        "not_found_warning": any("codec not found" in w for w in CURRENT["warnings"]),
        # `if _probed or discord.opus.is_loaded(): return` -- the guard itself,
        # not an observable (already_loaded short-circuits differently upstream).
        "returned_early": bool(case.get("probed") or case.get("already_loaded")),
    }


def main():
    load_ns()
    cases = mism = 0
    wanted = {"bundled", "plan"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = drive(r, r.get("func"))
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
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
