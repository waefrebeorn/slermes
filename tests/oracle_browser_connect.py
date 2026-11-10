#!/usr/bin/env python3
"""Oracle for port_browser_connect.c -- drives the LIVE browser_connect.py defs.

The harness echoes its inputs from C. The live functions are ast-extracted
because importing hermes_cli.browser_connect pulls in subprocess/socket/sqlite.
"""
import ast
import json
import re
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\browser_connect.py"
WANTED = ["_classify_default", "_first_present"]


def extract():
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    ns = {}
    # _classify_default reads this module global; ast-extraction does not carry
    # module-level Assign nodes, so seed it or the live call raises NameError.
    for node in tree.body:
        if isinstance(node, ast.Assign):
            for tgt in node.targets:
                if isinstance(tgt, ast.Name) and tgt.id == "UNSUPPORTED_CHANNEL":
                    exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    ns.setdefault("UNSUPPORTED_CHANNEL", "unsupported_channel")
    import os as _os
    ns["os"] = _os
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


# The module's UNSUPPORTED_CHANNEL constant
UNSUPPORTED_CHANNEL = "unsupported_channel"


def main():
    ns = extract()
    classify = ns["_classify_default"]
    first_present = ns["_first_present"]

    CHANNELS = ["beta", "dev", "canary", "nightly"]
    TABLE = [("chrome", "chrome"), ("msedge", "edge"), ("firefox", "firefox"),
             ("brave", "brave"), ("arc", "arc")]

    def match(value, needle):
        return bool(re.search(re.escape(needle), value or ""))

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

        if fn == "_classify_default":
            want = classify(c["in"], CHANNELS, TABLE, match)
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH _classify_default in={c['in']!r} C={got!r} PY={want!r}")

        elif fn == "_first_present":
            # Rebuild the same path list the harness used; both sides must see
            # the same filesystem, so use the repo-relative paths verbatim.
            label = c.get("case")
            paths = {
                "all-missing": ["", "/definitely/not/here"],
                "empty": [],
                "second-exists": ["/nope/nope", "tests/t_port_browser_connect.c"],
                "directory-not-file": ["tests"],
            }.get(label)
            if paths is None:
                mism += 1
                print(f"MISMATCH _first_present unknown case {label!r}")
                continue
            want = first_present(paths)
            got = c.get("out")
            # the C side reports the path as given; Python's isfile is relative
            # to the same cwd the harness ran in
            if got != want:
                mism += 1
                print(f"MISMATCH _first_present case={label} C={got!r} PY={want!r}")

    print(f"ORACLE browser_connect: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
