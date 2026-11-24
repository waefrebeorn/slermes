#!/usr/bin/env python3
"""Differential oracle for hermes_cli/windows_store_update.py (gate w25_windowssto_d94391).

The Store itself is a seam, so the oracle stands in for it: a fake ``winrt``
package tree (apartment, StoreContext, StorePackageUpdateState), a fake update
list and a fake request outcome are injected into ``sys.modules`` and the LIVE
``main()`` is run for each case.  What is compared is exactly what the desktop
reads off the child process -- the one JSON document on stdout and the exit
code -- so no part of perform/_execute/main is re-implemented here.

``_wait`` is replaced by identity: the async pump (Win32 message loop, 30 min
deadline) is the seam the fixtures stand in for.
"""
import argparse
import ast
import contextlib
import importlib.util
import inspect
import io
import json
import sys
import textwrap
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_windowssto_d94391"


def load_live():
    path = QUARRY / "hermes_cli" / "windows_store_update.py"
    spec = importlib.util.spec_from_file_location("_live_windows_store_update", path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["_live_windows_store_update"] = mod
    spec.loader.exec_module(mod)
    return mod


live = load_live()

WANTED = {"main_result", "constants"}


class _State:
    """One StorePackageUpdateState enum member; identity is the comparison."""

    def __init__(self, name):
        self.name = name

    def __repr__(self):
        return "State(%s)" % self.name


_STATES = {}


def _state(name):
    return _STATES.setdefault(name, _State(name))


def _pkg(full_name):
    return types.SimpleNamespace(id=types.SimpleNamespace(full_name=full_name))


def _update(full_name):
    return types.SimpleNamespace(package=_pkg(full_name))


def _fake_module(name, **attrs):
    mod = types.ModuleType(name)
    for key, value in attrs.items():
        setattr(mod, key, value)
    sys.modules[name] = mod
    if "." in name:
        parent_name, _, child = name.rpartition(".")
        parent = sys.modules.get(parent_name)
        if parent is not None:
            setattr(parent, child, mod)
    return mod


def _install_fakes(plan, calls):
    """A whole fake WinRT surface, wired to the plan's updates/outcome/exceptions."""
    packages = plan.get("packages")
    names = [p for p in packages if isinstance(p, str)] if isinstance(packages, list) else []
    overall = plan.get("overall_state") or "COMPLETED"
    request_error = plan.get("request_error")
    get_error = plan.get("get_error")
    init_error = plan.get("init_error")

    class Context:
        def get_app_and_optional_store_package_updates_async(self):
            calls.append(("query", None))
            if get_error:
                raise RuntimeError(get_error)
            return [_update(n) for n in names]

        def request_download_store_package_updates_async(self, updates):
            calls.append(("download", len(updates)))
            if request_error:
                raise RuntimeError(request_error)
            return types.SimpleNamespace(overall_state=_state(overall))

        def request_download_and_install_store_package_updates_async(self, updates):
            calls.append(("install", len(updates)))
            if request_error:
                raise RuntimeError(request_error)
            return types.SimpleNamespace(overall_state=_state(overall))

    context = Context()

    class StoreContext:
        @staticmethod
        def get_default():
            return context

    class StorePackageUpdateState:
        COMPLETED = _state("COMPLETED")

    def init_apartment(*_args):
        if init_error:
            raise RuntimeError(init_error)

    _fake_module("winrt")
    _fake_module(
        "winrt.runtime",
        ApartmentType=types.SimpleNamespace(SINGLE_THREADED=0),
        init_apartment=init_apartment,
        uninit_apartment=lambda: None,
    )
    _fake_module("winrt.runtime.interop", initialize_with_window=lambda _c, _h: None)
    _fake_module("winrt.windows")
    _fake_module("winrt.windows.services")
    _fake_module("winrt.windows.services.store",
                 StoreContext=StoreContext, StorePackageUpdateState=StorePackageUpdateState)
    _fake_module("winrt.windows.applicationmodel")
    _fake_module("winrt.windows.foundation")
    _fake_module("winrt.windows.foundation.collections")


def main_result(req):
    plan = req.get("plan")
    if not isinstance(plan, dict):
        return None  # the port's guard: no plan object, no run
    packages = plan.get("packages")
    if packages is not None and not isinstance(packages, list):
        return None
    calls = []
    _install_fakes(plan, calls)
    live._wait = lambda operation: operation
    argv = ["--mode", plan.get("mode") or "check"]
    hwnd = plan.get("hwnd")
    if hwnd:
        argv += ["--hwnd", str(hwnd)]
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        code = live.main(argv)
    return {"result": json.loads(buf.getvalue()), "exit": code}


def _function_source(name):
    """Source of a top-level function read off the file, not the (patched) object."""
    path = QUARRY / "hermes_cli" / "windows_store_update.py"
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == name:
            return node
    raise AssertionError("no %s in %s" % (name, path))


def constants():
    args = {}
    for node in ast.walk(_function_source("main")):
        if isinstance(node, ast.Call) and getattr(node.func, "attr", "") == "add_argument":
            opts = [a.value for a in node.args if isinstance(a, ast.Constant)]
            kwargs = {}
            for k in node.keywords:
                if isinstance(k.value, ast.Name):
                    kwargs[k.arg] = k.value.id
                    continue
                try:
                    kwargs[k.arg] = ast.literal_eval(k.value)
                except (ValueError, TypeError):
                    pass
            for opt in opts:
                args[opt] = kwargs
    mode_kw = args["--mode"]
    hwnd_kw = args["--hwnd"]
    wait_ints = sorted(
        n.value for n in ast.walk(_function_source("_wait"))
        if isinstance(n, ast.Constant) and isinstance(n.value, int) and not isinstance(n.value, bool)
    )
    return {
        "modes": list(mode_kw["choices"]),
        "default_mode": mode_kw["default"],
        "hwnd_flag": "--hwnd",
        "hwnd_type": hwnd_kw["type"] if isinstance(hwnd_kw["type"], str) else hwnd_kw["type"].__name__,
        "timeout_s": wait_ints[-1],
        "poll_ms": wait_ints[0],
    }


def expected(req):
    f = req.get("func")
    if f == "main_result":
        return main_result(req)
    if f == "constants":
        return constants()
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
