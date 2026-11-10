#!/usr/bin/env python3
"""Oracle for port_service_manager.c -- drives the LIVE service_manager.py defs.

The harness echoes its inputs from C. The live functions are ast-extracted
because importing hermes_cli.service_manager pulls in subprocess/shutil.
"""
import ast
import json
import re
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\service_manager.py"
WANTED = ["validate_profile_name", "_profile_from_service"]


def extract():
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    ns = {}
    # `re` must be present BEFORE the module-level Assign nodes run: the source
    # has _VALID_PROFILE_RE = re.compile(...), and exec of that Assign raises
    # NameError without it.
    import re as _re
    ns.setdefault("re", _re)
    # module constants the live bodies read
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

        if fn == "validate_profile_name":
            name = c["in"] if c["in"] is not None else ""
            want_rc, want_err = 0, ""
            try:
                ns["validate_profile_name"](name)
            except ValueError as e:
                want_rc, want_err = -1, str(e)
            got_rc = c.get("rc")
            got_err = c.get("err") or ""
            if got_rc != want_rc or (want_rc == -1 and got_err != want_err):
                mism += 1
                print(f"MISMATCH validate_profile_name in={c['in']!r} "
                      f"C=({got_rc},{got_err!r}) PY=({want_rc},{want_err!r})")

        elif fn == "_profile_from_service":
            want = ns["_profile_from_service"](c["in"] or "")
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH _profile_from_service in={c['in']!r} C={got!r} PY={want!r}")

    print(f"ORACLE service_manager: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
