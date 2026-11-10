#!/usr/bin/env python3
"""Oracle for port_container_boot.c -- drives the LIVE container_boot.py defs.

The live functions are ast-extracted; the module's /proc + s6 I/O is not
imported. `_strip_container_argv_prefix` is extracted together with the module
constants it reads, so `Path` is seeded for the basename comparison.
"""
import ast
import json
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\container_boot.py"
WANTED = ["_strip_container_argv_prefix", "_is_legacy_gateway_run_request",
          "_is_dashboard_container", "boot_notice"]


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {"Path": Path}
    tree = ast.parse(src)
    # module-level constants the defs read
    for node in tree.body:
        if isinstance(node, ast.Assign):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    strips = {
        "wrapper-mid": ["s6-init", "/etc/s6/main-wrapper.sh", "gateway", "run"],
        "wrapper-first": ["main-wrapper.sh", "gateway", "run"],
        "init-defensive": ["init", "gateway", "run"],
        "entrypoint": ["entrypoint-dispatch.sh", "dashboard"],
        "plain": ["gateway", "run"],
        "wrapper-with-flag": ["/opt/hermes/bin/main-wrapper.sh", "--no-supervise", "gateway", "run"],
        "empty": [],
    }
    runs = {
        "legacy-run": ["gateway", "run"],
        "legacy-via-wrapper": ["s6-init", "/etc/s6/main-wrapper.sh", "gateway", "run"],
        "legacy-no-supervise": ["/opt/hermes/bin/main-wrapper.sh", "--no-supervise", "gateway", "run"],
        "not-legacy": ["entrypoint-dispatch.sh", "dashboard"],
    }
    dash = {
        "dashboard": ["entrypoint-dispatch.sh", "dashboard"],
        "not-dashboard": ["gateway", "run"],
        "dashboard-substr-not-first": ["s6-init", "/etc/s6/main-wrapper.sh", "gateway", "run"],
    }
    notices = {
        "three": ["beta", "alpha", "gamma"],
        "empty": [],
        "one": ["solo"],
    }

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

        if fn == "_strip_container_argv_prefix":
            want = ns["_strip_container_argv_prefix"](strips.get(c.get("case"), []))
            got = c.get("out") or []
            if got != want:
                mism += 1
                print(f"MISMATCH _strip_container_argv_prefix case={c.get('case')} C={got!r} PY={want!r}")

        elif fn == "_is_legacy_gateway_run_request":
            want = bool(ns["_is_legacy_gateway_run_request"](runs.get(c.get("case"), [])))
            got = bool(c.get("out"))
            if got != want:
                mism += 1
                print(f"MISMATCH _is_legacy_gateway_run_request case={c.get('case')} C={got!r} PY={want!r}")

        elif fn == "_is_dashboard_container":
            want = bool(ns["_is_dashboard_container"](dash.get(c.get("case"), [])))
            got = bool(c.get("out"))
            if got != want:
                mism += 1
                print(f"MISMATCH _is_dashboard_container case={c.get('case')} C={got!r} PY={want!r}")

        elif fn == "boot_notice":
            want = ns["boot_notice"](notices.get(c.get("case"), []))
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH boot_notice case={c.get('case')} C={got!r} PY={want!r}")

    print(f"ORACLE container_boot: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
