#!/usr/bin/env python3
"""Oracle for the w12_profilesca_d12947 port of hermes_cli/profiles_cache.py.

Loads the LIVE module from the quarry (sys.path insert + parent-package
bootstrap + sys.modules registration) and compares the harness `out` for the
derived-value cache, the str() coercion hook and json_safe_dumps.
"""
import copy
import importlib
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
importlib.import_module("hermes_cli")            # parent-package bootstrap
_SPEC = importlib.util.spec_from_file_location(
    "_w12_profilesca_live", QUARRY + "/hermes_cli/profiles_cache.py")
live = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = live                    # sys.modules registration
_SPEC.loader.exec_module(live)

_NAN = "__W12_NAN__"


def map_nan(v):
    """Inject a NaN at the sentinel so the allow_nan=False path is exercised."""
    if isinstance(v, str) and v == _NAN:
        return float("nan")
    if isinstance(v, dict):
        return {k: map_nan(x) for k, x in v.items()}
    if isinstance(v, list):
        return [map_nan(x) for x in v]
    return v


def want(r):
    fn = r.get("func")
    if fn == "cached_profile_read":
        if bool(r.get("reset", False)):
            live._PROFILE_FILE_CACHE.clear()
        live._PROFILE_FILE_CACHE_MAX = int(r.get("max", 512))
        sig = r.get("signature")
        path = r.get("path", "")
        kind = r.get("kind", "")
        key = (str(path), kind)
        before = len(live._PROFILE_FILE_CACHE)
        hit = False
        if sig is not None:
            cached = live._PROFILE_FILE_CACHE.get(key)
            hit = cached is not None and cached[0] == sig
        calls = [0]

        def compute():
            calls[0] += 1
            return copy.deepcopy(r.get("value"))

        res = live._cached_profile_read(path, kind, compute,
                                        signature_of=lambda p: sig)
        cleared = bool(sig is not None and not hit
                       and before >= live._PROFILE_FILE_CACHE_MAX)
        return {"value": res, "computed": calls[0] > 0, "cleared": cleared}
    if fn == "yaml_scalar_to_json":
        return live.yaml_scalar_to_json(r.get("value"))
    if fn == "json_safe_dumps":
        return live.json_safe_dumps(map_nan(r.get("value")))
    return None


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if r.get("func") is None:
            continue
        n += 1
        got = r.get("out")
        try:
            expected = want(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case %d: live raised %r" % (r.get("func"), n, exc))
            continue
        if got != expected:
            mism += 1
            if mism <= 12:
                print("MISMATCH %s case %d C=%r PY=%r" % (r.get("func"), n, got, expected))
    print("ORACLE w12_profilesca_d12947: %d cases, %d mismatches" % (n, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
