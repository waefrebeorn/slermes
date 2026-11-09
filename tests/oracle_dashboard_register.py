#!/usr/bin/env python3
"""Oracle for port_dashboard_register.c -- drives the LIVE dashboard_register.py defs.

The harness echoes its inputs from C. The live functions are ast-extracted
because importing hermes_cli.dashboard_register pulls in urllib.
"""
import ast
import json
import sys
import types
from urllib.parse import urlparse

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\dashboard_register.py"
WANTED = ["_resolve_portal_base_url", "_public_url_from_redirect"]
DEFAULT_PORTAL = "https://portal.nousresearch.com"


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {}
    # the live body calls urlparse and reads the module-level _DEFAULT_PORTAL;
    # ast-extraction carries neither the module import nor its constants.
    from urllib.parse import urlparse as _urlparse
    ns.setdefault("urlparse", _urlparse)
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign):
            for tgt in node.targets:
                if isinstance(tgt, ast.Name) and tgt.id == "_DEFAULT_PORTAL":
                    try:
                        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
                    except Exception:
                        pass
    ns.setdefault("_DEFAULT_PORTAL", DEFAULT_PORTAL)
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        fn = c.get("fn")
        cases += 1

        if fn == "_public_url_from_redirect":
            uri = c["in"] if c["in"] is not None else ""
            want = ns["_public_url_from_redirect"](uri)
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH _public_url_from_redirect in={uri!r} C={got!r} PY={want!r}")

        elif fn == "_resolve_portal_base_url":
            label = c.get("case")
            cfgs = {
                "override": ("https://custom.example/", None, DEFAULT_PORTAL),
                "override-trailing": ("https://custom.example///", None, DEFAULT_PORTAL),
                "stored": (None, "https://stored.example/", DEFAULT_PORTAL),
                "default": (None, None, DEFAULT_PORTAL),
                "override-blank": ("   ", "https://stored.example", DEFAULT_PORTAL),
                "stored-blank": (None, "  ", DEFAULT_PORTAL),
            }
            ovr, stored, dflt = cfgs.get(label, (None, None, DEFAULT_PORTAL))
            # The live body does `from hermes_cli.auth import ...` INSIDE the
            # function, which overrides an ns[] seed at call time. Put the fake
            # on sys.modules instead (plus a parent package) so the import in the
            # body resolves to it.
            auth = types.ModuleType("hermes_cli.auth")
            auth.DEFAULT_NOUS_PORTAL_URL = dflt
            auth.get_provider_auth_state = (
                (lambda v: (lambda name: {"portal_base_url": v} if v else None))(stored)
            )
            sys.modules["hermes_cli.auth"] = auth
            parent = sys.modules.get("hermes_cli") or types.ModuleType("hermes_cli")
            parent.auth = auth
            sys.modules["hermes_cli"] = parent
            want = ns["_resolve_portal_base_url"](ovr)
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH _resolve_portal_base_url case={label} C={got!r} PY={want!r}")

    print(f"ORACLE dashboard_register: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
