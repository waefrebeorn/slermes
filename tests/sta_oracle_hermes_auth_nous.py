#!/usr/bin/env python3
"""Oracle for the pure core of hermes_cli/auth_nous.py.

The module cannot import (heavy chain), so the eight live function definitions
are extracted verbatim from the source by ast and exec'd into a namespace
holding the exact module-level constants, plus the two impure seams stubbed
(_nous_err, _nous_inference_env_override). Every input is read from the C
harness's emitted case line — no re-typed fixture tables.
"""
import ast
import hashlib
import json
import re
import sys
from datetime import datetime, UTC
from types import SimpleNamespace
from urllib.parse import urlparse

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/auth_nous.py"
WANTED = {
    "_token_fingerprint", "_iso_after", "_nous_inference_host_allowed",
    "_scope_values", "_unusable_invoke_jwt_error", "_migrate_stale_nous_portal_url",
    "_nous_effective_provider_state", "_portal_host",
}

override_holder = {"val": None}
ns = {
    "hashlib": hashlib, "datetime": datetime, "UTC": UTC, "urlparse": urlparse,
    "logger": SimpleNamespace(warning=lambda *a, **k: None, info=lambda *a, **k: None),
    "DEFAULT_NOUS_PORTAL_URL": "https://portal.nousresearch.com",
    "_UNUSABLE_JWT_RELOGIN": "Re-authenticate with: hermes auth add nous",
    "_ALLOWED_NOUS_INFERENCE_HOSTS": frozenset({
        "inference-api.nousresearch.com", "welcome-api.nousresearch.com"}),
    "_NOUS_STALE_PORTAL_HOSTS": frozenset({"api.nousresearch.com"}),
    "_NOUS_EFFECTIVE_STATE_IGNORED_KEYS": frozenset({"expires_in", "agent_key_expires_in"}),
    "_nous_err": lambda msg, code, relogin=None: SimpleNamespace(message=msg, code=code),
    "_nous_inference_env_override": lambda: override_holder["val"],
}

tree = ast.parse(open(SRC, encoding="utf-8").read())
for n in tree.body:
    if isinstance(n, ast.FunctionDef) and n.name in WANTED:
        mod = ast.Module(body=[n], type_ignores=[])
        exec(compile(ast.fix_missing_locations(mod), "auth_nous.py", "exec"), ns)


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE auth_nous: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        fam, case = c["family"], c["case"]
        try:
            if fam == "fp":
                want = ns["_token_fingerprint"](c["in"])
                if c["out"] != want:
                    mism += 1
                    print(f"MISMATCH {case}: C={c['out']!r} PY={want!r}")
            elif fam == "iso":
                want = ns["_iso_after"](datetime.fromtimestamp(c["now"], tz=UTC), c["ttl"])
                if c["out"] != want:
                    mism += 1
                    print(f"MISMATCH {case}: C={c['out']!r} PY={want!r}")
            elif fam == "host":
                override_holder["val"] = c["ov"]
                want = bool(ns["_nous_inference_host_allowed"](c["host"]))
                if bool(c["ret"]) != want:
                    mism += 1
                    print(f"MISMATCH {case}: C={c['ret']} PY={want} host={c['host']!r}")
            elif fam == "scope":
                got = set(c["out"])
                want = ns["_scope_values"](c["in"])
                if got != want:
                    mism += 1
                    print(f"MISMATCH {case}: C={sorted(got)} PY={sorted(want)}")
            elif fam == "unusable":
                want_err = ns["_unusable_invoke_jwt_error"](c["r"], no_refresh_token=c["nr"])
                want_msg = getattr(want_err, "message", str(want_err))
                want_code = getattr(want_err, "code", c["r"])
                if c["msg"] != want_msg:
                    mism += 1
                    print(f"MISMATCH {case}.msg: C={c['msg']!r} PY={want_msg!r}")
                if c["code"] != want_code:
                    mism += 1
                    print(f"MISMATCH {case}.code: C={c['code']!r} PY={want_code!r}")
            elif fam == "migrate":
                prov = c["in"]  # already decoded
                want = dict(prov) if isinstance(prov, dict) else prov
                ns["_migrate_stale_nous_portal_url"](want)
                if c["out"] != want:
                    mism += 1
                    print(f"MISMATCH {case}: C={c['out']} PY={want}")
            elif fam == "effective":
                want = ns["_nous_effective_provider_state"](c["in"])
                if c["out"] != want:
                    mism += 1
                    print(f"MISMATCH {case}: C={c['out']} PY={want}")
            elif fam == "portal_host":
                want = ns["_portal_host"](c["in"])
                if c["out"] != want:
                    mism += 1
                    print(f"MISMATCH {case}: C={c['out']!r} PY={want!r}")
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print(f"ERROR {case} [{fam}]: {exc!r}")

    print(f"ORACLE auth_nous: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
