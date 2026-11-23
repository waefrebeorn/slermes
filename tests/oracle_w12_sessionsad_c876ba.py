#!/usr/bin/env python3
"""Oracle for the w12_sessionsad_c876ba port of hermes_cli/sessions_adopt.py.

Loads the LIVE module from the quarry (sys.path insert + parent-package
bootstrap + sys.modules registration) and compares `_refusal` / `_unreadable_store`.
The two in-function imports of `_unreadable_store` are fed recording stubs so
the exception classification is injected from the fixture.
"""
import importlib
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
importlib.import_module("hermes_cli")            # parent-package bootstrap
_SPEC = importlib.util.spec_from_file_location(
    "_w12_sessionsad_live", QUARRY + "/hermes_cli/sessions_adopt.py")
live = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = live                    # sys.modules registration
_SPEC.loader.exec_module(live)


def want(r):
    fn = r.get("func")
    if fn == "refusal":
        exc = RuntimeError(r.get("reason", ""))
        exc.data = {"field": r.get("field"), "detail": r.get("detail")}
        return live._refusal(exc)
    if fn == "unreadable_store":
        kind = r.get("kind")
        ss = types.ModuleType("sqlite_support")
        ss.schema_not_built = lambda exc: kind == "schema"
        se = types.ModuleType("storage_errors")
        se.is_lock_contention = lambda exc: kind == "lock"
        old_ss = sys.modules.get("sqlite_support")
        old_se = sys.modules.get("storage_errors")
        sys.modules["sqlite_support"] = ss
        sys.modules["storage_errors"] = se
        try:
            return live._unreadable_store(
                RuntimeError(r.get("exc_text", "")), r.get("store", ""))
        finally:
            if old_ss is not None:
                sys.modules["sqlite_support"] = old_ss
            else:
                sys.modules.pop("sqlite_support", None)
            if old_se is not None:
                sys.modules["storage_errors"] = old_se
            else:
                sys.modules.pop("storage_errors", None)
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
    print("ORACLE w12_sessionsad_c876ba: %d cases, %d mismatches" % (n, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
