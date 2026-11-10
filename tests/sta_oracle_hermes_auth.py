#!/usr/bin/env python3
"""Oracle for the pure URL/key-shape predicates of hermes_cli/auth.py.

The module cannot import (heavy registry), so the five live function
definitions are ast-extracted verbatim and exec'd with the exact module
constants + urlparse. Inputs come from the C harness's emitted case lines.
"""
import ast
import json
import sys
from urllib.parse import urlparse

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/auth.py"
WANTED = {"is_actual_local_base_url", "normalize_actual_base_url",
          "_is_placeholder_shape", "_matches_key_prefix", "looks_like_openrouter_key"}

ns = {
    "urlparse": urlparse,
    "DEFAULT_ACTUAL_BASE_URL": "https://api.actual.inc/v1",
    "_PLACEHOLDER_KEY_PREFIXES": ("sk-", "ghp_", "hf_"),
    "KNOWN_PROVIDER_KEY_PREFIXES": {"openrouter": ("sk-or-",)},
}

tree = ast.parse(open(SRC, encoding="utf-8").read())
for n in tree.body:
    if isinstance(n, ast.FunctionDef) and n.name in WANTED:
        exec(compile(ast.fix_missing_locations(ast.Module(body=[n], type_ignores=[])),
                     "auth.py", "exec"), ns)


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
        print("ORACLE auth_urls: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        fam, case = c["family"], c["case"]
        try:
            if fam == "local":
                want = ns["is_actual_local_base_url"](c["in"])
                if bool(c["ret"]) != bool(want):
                    mism += 1
                    print(f"MISMATCH {case}: C={c['ret']} PY={want} in={c['in']!r}")
            elif fam == "normalize":
                want = ns["normalize_actual_base_url"](c["in"])
                if c["out"] != want:
                    mism += 1
                    print(f"MISMATCH {case}: C={c['out']!r} PY={want!r} in={c['in']!r}")
            elif fam == "placeholder":
                if c["in"] is None:
                    continue  # Python would raise on None.lower(); caller bug
                want = ns["_is_placeholder_shape"](c["in"])
                if bool(c["ret"]) != bool(want):
                    mism += 1
                    print(f"MISMATCH {case}: C={c['ret']} PY={want} in={c['in']!r}")
            elif fam == "keyprefix":
                want = ns["_matches_key_prefix"](c["prov"], c["val"])
                if bool(c["ret"]) != bool(want):
                    mism += 1
                    print(f"MISMATCH {case}: C={c['ret']} PY={want} "
                          f"prov={c['prov']!r} val={c['val']!r}")
            elif fam == "openrouter":
                want = ns["looks_like_openrouter_key"](c["in"])
                if bool(c["ret"]) != bool(want):
                    mism += 1
                    print(f"MISMATCH {case}: C={c['ret']} PY={want} in={c['in']!r}")
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print(f"ERROR {case} [{fam}]: {exc!r}")

    print(f"ORACLE auth_urls: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
