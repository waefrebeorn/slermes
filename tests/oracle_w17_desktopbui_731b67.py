#!/usr/bin/env python3
"""Differential oracle for gate w17_desktopbui_731b67 (hermes_cli/desktop_build_lock.py).

The LIVE DesktopBuildLock.__init__ computes the keyed lock path; hermes_constants'
get_default_hermes_root is stubbed because the checkout key does not depend on it.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if not (Path(QUARRY) / "hermes_cli" / "desktop_build_lock.py").is_file():
    QUARRY = str(Path(__file__).resolve().parent.parent.parent / "hermes-agent")
sys.path.insert(0, QUARRY)
for _pkg in ("hermes_cli",):
    if _pkg not in sys.modules:
        _m = types.ModuleType(_pkg)
        _m.__path__ = [str(Path(QUARRY) / _pkg)]
        sys.modules[_pkg] = _m

_hc = types.ModuleType("hermes_constants")
_hc.get_default_hermes_root = lambda: Path("C:/hermesroot")
sys.modules["hermes_constants"] = _hc

_SPEC = importlib.util.spec_from_file_location(
    "w17_desktopbui_live", str(Path(QUARRY) / "hermes_cli" / "desktop_build_lock.py"))
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, list): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def _name(root):
    return mod.DesktopBuildLock(Path(root)).path.name


def expected(rec):
    f = rec.get("func")
    root = rec.get("project_root") or ""
    if f == "lock_name":
        return _name(root)
    if f == "checkout_key":
        name = _name(root)
        return name[len("desktop-build-"):-len(".lock")]
    raise RuntimeError("unknown func " + str(f))


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        got = rec.get("out")
        want = expected(rec)
        if _canon(got) != _canon(want):
            mism += 1
            if mism <= 10:
                print("MISMATCH %s root=%r\n  got : %s\n  want: %s" %
                      (rec.get("func"), rec.get("project_root"), json.dumps(got, ensure_ascii=False),
                       json.dumps(want, ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w17_desktopbui_731b67: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
