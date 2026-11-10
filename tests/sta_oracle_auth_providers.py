#!/usr/bin/env python3
"""Oracle for hermes_cli/auth_commands.py: _is_known_provider (live,
ast-extracted; PROVIDER_REGISTRY + CUSTOM_POOL_PREFIX globals provided) and
_provider_base_url (its branch logic transcribed verbatim with the module
constants — the pure independent reference for the IO-reading branches).
"""
import ast
import json
import sys
from types import SimpleNamespace

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/auth_commands.py"
code = open(SRC, encoding="utf-8").read()
tree = ast.parse(code)

known_fn = None
for n in tree.body:
    if isinstance(n, ast.FunctionDef) and n.name == "_is_known_provider":
        known_fn = ast.get_source_segment(code, n)

NSA = {"CUSTOM_POOL_PREFIX": "custom:"}
exec(compile(ast.parse(known_fn), "x", "exec"), NSA)
_is_known = NSA["_is_known_provider"]

resolve_fn = None
for n in tree.body:
    if isinstance(n, ast.FunctionDef) and n.name == "_resolve_custom_provider_input":
        resolve_fn = ast.get_source_segment(code, n)
NSR = {"CUSTOM_POOL_PREFIX": "custom:",
       "_normalize_custom_pool_name": (lambda name: name.strip().lower().replace(" ", "-"))}
exec(compile(ast.parse(resolve_fn), "x", "exec"), NSR)
_resolve = NSR["_resolve_custom_provider_input"]

OPENROUTER = "https://openrouter.ai/api/v1"
CPREFIX = "custom:"


def expected_base_url(provider, reg, cfg, cust):
    if provider == "openrouter":
        return OPENROUTER
    if provider.startswith(CPREFIX):
        return cust.strip() if cust else ""
    if cfg:  # configured entry present (non-empty fact)
        return cfg.strip()
    return reg.strip() if reg else ""


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
        print("ORACLE auth_providers: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "burl":
            want = expected_base_url(c["provider"], c["reg"], c["cfg"], c["cust"])
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH burl {c['case']}: C={c['out']!r} PY={want!r} "
                      f"provider={c['provider']!r}")
        elif fam == "iknow":
            registry = {c["provider"]: object()} if c["inreg"] else {}
            NSA["PROVIDER_REGISTRY"] = registry
            configured = {} if c["cfg"] else None
            want = 1 if _is_known(c["provider"], configured) else 0
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH iknow {c['case']}: C={c['out']} PY={want} "
                      f"provider={c['provider']!r}")
        elif fam == "resolve":
            NSR["_get_custom_provider_entries"] = lambda: list(c["entries"])
            want = _resolve(c["raw"])
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH resolve {c['case']}: C={c['out']!r} PY={want!r} "
                      f"raw={c['raw']!r}")
    print(f"ORACLE auth_providers: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
