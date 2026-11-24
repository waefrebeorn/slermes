#!/usr/bin/env python3
"""Differential oracle for gate w17_configset_671393 (hermes_cli/config_set_scalars.py)."""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if not (Path(QUARRY) / "hermes_cli" / "config_set_scalars.py").is_file():
    QUARRY = str(Path(__file__).resolve().parent.parent.parent / "hermes-agent")
sys.path.insert(0, QUARRY)
for _pkg in ("hermes_cli",):
    if _pkg not in sys.modules:
        _m = types.ModuleType(_pkg)
        _m.__path__ = [str(Path(QUARRY) / _pkg)]
        sys.modules[_pkg] = _m

_SPEC = importlib.util.spec_from_file_location(
    "w17_configset_live", str(Path(QUARRY) / "hermes_cli" / "config_set_scalars.py"))
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


def expected(rec):
    f = rec.get("func")
    v = rec.get("value")
    if f == "coerce_int":
        return mod._coerce_int(v)
    if f == "coerce_float":
        return mod._coerce_float(v)
    if f == "quoted_bool":
        try:
            return mod._quoted_bool(v)
        except TypeError:
            return None       # live _quoted_bool(None) raises; C maps NULL to null
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
                print("MISMATCH %s value=%r\n  got : %s\n  want: %s" %
                      (rec.get("func"), rec.get("value"), json.dumps(got, ensure_ascii=False),
                       json.dumps(want, ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w17_configset_671393: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
