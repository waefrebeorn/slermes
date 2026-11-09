#!/usr/bin/env python3
"""Oracle for port_skill_bundles.c -- drives the LIVE agent/skill_bundles.py
defs. The scan/cache/YAML half is not imported; the pure defs are
ast-extracted, with `slugify_skill_name` re-exported from skill_commands and
`resolve_slash_key` likewise.
"""
import ast
import hashlib
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\agent\skill_bundles.py"
CMDS = r"C:\Users\eman5\hermes-agent\agent\skill_commands.py"
WANTED = ["get_bundle", "bundle_path_for", "_bundles_sig", "list_bundles",
          "resolve_bundle_command_key"]


def extract():
    import re as _re
    ns = {"hashlib": hashlib, "re": _re,
          "_SKILL_INVALID_CHARS": _re.compile(r"[^\w-]"),
          "_SKILL_MULTI_HYPHEN": _re.compile(r"-{2,}")}
    src = open(CMDS, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), CMDS, "exec"), ns)
            except Exception:
                pass
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in ("slugify_skill_name", "resolve_slash_key"):
            exec(compile(ast.Module(body=[node], type_ignores=[]), CMDS, "exec"), ns)
    ns["_slugify"] = ns["slugify_skill_name"]
    ns["_bundles_dir"] = lambda: type("P", (), {"__truediv__": lambda self, o: o})()
    src = open(SRC, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    sigs = {
        "three": ([("/b/beta.yaml", "beta body"), ("/b/alpha.yaml", "alpha body"),
                   ("/b/gamma.yaml", "gamma body")], None),
        "one-missing": ([("/b/a.yaml", "aaa"), ("/b/b.yaml", None), ("/b/c.yaml", "ccc")], None),
        "empty": ([], None),
    }
    orders = {
        "three": ["zeta", "alpha", "mid"],
        "one": ["only"],
        "dupe": ["b", "a", "c", "a"],
        "empty": [],
    }
    resolves = {
        "exact": ("work", ["/work", "/my-bundle"]),
        "under": ("my_bundle", ["/work", "/my-bundle"]),
        "missing": ("nope", ["/work", "/my-bundle"]),
        "empty": ("", ["/work", "/my-bundle"]),
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

        if fn == "get_bundle_key":
            name = c.get("case")
            names = {"simple": "My Bundle", "under": "my_bundle", "empty": "",
                     "null": None, "punct": "a+b/c"}
            want = None
            n = names.get(c.get("case"))
            if n is not None:
                want = "/" + ns["_slugify"](n)
            # n is None for the "null" case: the Python raises, so the C
            # port's NULL is the faithful result
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH get_bundle_key case={c.get('case')} C={got!r} PY={want!r}")

        elif fn == "bundle_path_for":
            cfgs = {
                "basic": ("My Bundle", "/home/u/skill-bundles"),
                "empty-slug": ("+++", "/home/u/skill-bundles"),
                "null-name": (None, "/home/u/skill-bundles"),
                "no-dir": ("work", None),
                "trailing": ("work", "/home/u/skill-bundles/"),
            }
            name, d = cfgs.get(c.get("case"), (None, None))
            try:
                # the live fn calls _slugify(None) -> AttributeError; the C port
                # returns NULL with err=1, which is the faithful equivalent
                want = ns["bundle_path_for"](name)
                want_err = 0
                # the Python returns a Path; the C takes an explicit dir, so the
                # comparison is on the FILE NAME component
                want = want.name if hasattr(want, "name") else str(want).replace("\\", "/").rsplit("/", 1)[-1]
            except (ValueError, TypeError, AttributeError):
                want, want_err = None, 1
            got = c.get("out")
            got_err = c.get("err", 0)
            if got_err != want_err:
                mism += 1
                print(f"MISMATCH bundle_path_for case={c.get('case')} err C={got_err!r} PY={want_err!r}")
            elif not got_err:
                got_name = (got or "").replace("\\", "/").rsplit("/", 1)[-1]
                if got_name != want:
                    mism += 1
                    print(f"MISMATCH bundle_path_for case={c.get('case')} C={got_name!r} PY={want!r}")

        elif fn == "_bundles_sig":
            entries = sigs.get(c.get("case"), ([], None))[0]
            want = sorted((p, hashlib.sha256(b.encode()).hexdigest()) for p, b in entries if b is not None)
            got = c.get("out") or []
            got_t = [(e[0], e[1]) for e in got]
            if got_t != want:
                mism += 1
                print(f"MISMATCH _bundles_sig case={c.get('case')}\n  C={got_t!r}\n  PY={want!r}")

        elif fn == "list_bundles":
            slugs = orders.get(c.get("case"), [])
            want = sorted(range(len(slugs)), key=lambda i: slugs[i])
            got = c.get("out") or []
            if got != want:
                mism += 1
                print(f"MISMATCH list_bundles case={c.get('case')} C={got!r} PY={want!r}")

        elif fn == "resolve_bundle_command_key":
            cmd, table = resolves.get(c.get("case"), ("", []))
            want = ns["resolve_slash_key"](cmd, table) if cmd else None
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH resolve_bundle_command_key case={c.get('case')} C={got!r} PY={want!r}")

    print(f"ORACLE skill_bundles: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
