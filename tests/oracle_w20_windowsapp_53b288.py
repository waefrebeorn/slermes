#!/usr/bin/env python3
"""Differential oracle for hermes_cli/windows_appinstaller_update.py
(w20_windowsapp_53b288).

The live module runs for real: `main()` is called with a stub
winrt.windows.applicationmodel projection (Package.current, the App Installer
source and the async check), its stdout is captured and the printed JSON plus
the returned exit code are compared. The enum is a real IntEnum with the
upstream member values, so an out-of-range availability raises the same
ValueError text the live module reports.
"""
import contextlib
import enum
import importlib.util
import io
import json
import os
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

NAME = "w20_windowsapp_53b288_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "hermes_cli", "windows_appinstaller_update.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


class PackageUpdateAvailability(enum.IntEnum):
    UNKNOWN = 0
    NO_UPDATES = 1
    AVAILABLE = 2
    REQUIRED = 3
    ERROR = 4


class _FakeOSError(OSError):
    def __init__(self, text, winerror=None):
        super().__init__(text)
        self.winerror = winerror
        self._text = text

    def __str__(self):
        return self._text


def _install_winrt(spec):
    mod = types.ModuleType("winrt.windows.applicationmodel")
    for pkg in ("winrt", "winrt.windows"):
        if pkg not in sys.modules:
            m = types.ModuleType(pkg)
            m.__path__ = []
            sys.modules[pkg] = m
    err = spec.get("import_error")
    if err:
        def _missing(name, _msg=err):
            raise ImportError(_msg)
        mod.__getattr__ = _missing
        sys.modules["winrt.windows.applicationmodel"] = mod
        return

    inst = SimpleNamespace()
    ident = spec.get("identity")
    src = spec.get("source")

    if ident is None:
        # `Package` is the projection class, so `Package.current` resolves on it
        Package = SimpleNamespace(current=inst)
    else:
        text = ident.get("str", "")
        winerror = ident.get("winerror")
        kind = ident.get("type", "OSError")

        class _Package:
            def __getattr__(self, name):
                if name == "current":
                    if kind == "RuntimeError":
                        raise RuntimeError(text)
                    raise _FakeOSError(text, winerror)
                raise AttributeError(name)

        Package = _Package()

    inst.get_app_installer_info = lambda: (
        None if src is None else SimpleNamespace(uri=SimpleNamespace(absolute_uri=src.get("uri"))))

    def _check():
        if spec.get("check_error") is not None:
            raise RuntimeError(spec["check_error"])
        value = spec.get("availability")
        return SimpleNamespace(get=lambda: SimpleNamespace(availability=value))

    inst.check_update_availability_async = _check
    mod.Package = Package
    mod.PackageUpdateAvailability = PackageUpdateAvailability
    sys.modules["winrt.windows.applicationmodel"] = mod


def _run_expected(spec):
    spec = spec if isinstance(spec, dict) else {}
    _install_winrt(spec)
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        rc = live.main()
    return {"exit": rc, "payload": json.loads(buf.getvalue())}


def expected(r):
    f = r.get("func")
    if f == "run":
        return _run_expected(r.get("spec"))
    if f == "availability_name":
        try:
            return PackageUpdateAvailability(r.get("value")).name
        except ValueError:
            return None
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_windowsapp_53b288: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
