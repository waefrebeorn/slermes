#!/usr/bin/env python3
"""Oracle for the w12_uipreferen_26932e port of hermes_cli/ui_preferences.py.

Loads the LIVE module from the quarry (sys.path insert + parent-package
bootstrap + sys.modules registration) and compares the three pure store
helpers: _sanitize, _merge and _validate.
"""
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
    "_w12_uipreferen_live", QUARRY + "/hermes_cli/ui_preferences.py")
live = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = live                    # sys.modules registration
_SPEC.loader.exec_module(live)


def want(r):
    fn = r.get("func")
    if fn == "sanitize":
        return live._sanitize(r.get("values"))
    if fn == "merge":
        return live._merge(r.get("current") or {}, r.get("changes") or {},
                           bool(r.get("seed", False)))
    if fn == "validate":
        try:
            live._validate(r.get("changes") or {})
            return {"ok": True, "error": None}
        except live.PreferencesError as exc:
            return {"ok": False, "error": str(exc)}
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
    print("ORACLE w12_uipreferen_26932e: %d cases, %d mismatches" % (n, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
