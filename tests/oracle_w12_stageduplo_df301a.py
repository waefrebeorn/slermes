#!/usr/bin/env python3
"""Oracle for the w12_stageduplo_df301a port of hermes_cli/staged_uploads.py.

Loads the LIVE module from the quarry (sys.path insert + parent-package
bootstrap + sys.modules registration); get_install_id is injected per case.
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
    "_w12_stageduplo_live", QUARRY + "/hermes_cli/staged_uploads.py")
live = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = live                    # sys.modules registration
_SPEC.loader.exec_module(live)


def want(r):
    fn = r.get("func")
    if fn == "safe_filename":
        return live._safe_filename(r.get("value"))
    if fn == "parse_staged_upload":
        live.get_install_id = lambda: r.get("install_id")
        try:
            home, path = live.parse_staged_upload(r.get("descriptor"))
            return {"home": home, "path": path}
        except Exception as exc:  # noqa: BLE001
            return {"error": str(exc)}
    if fn == "staged_upload_descriptor":
        live.get_install_id = lambda: r.get("install_id")
        return live.staged_upload_descriptor(r.get("path", ""),
                                             r.get("profile_home", ""))
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
    print("ORACLE w12_stageduplo_df301a: %d cases, %d mismatches" % (n, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
