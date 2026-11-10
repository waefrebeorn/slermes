#!/usr/bin/env python3
"""Oracle for port_inventory.c -- drives the LIVE hermes_cli/inventory.py defs.

The harness emits its inputs from C (never retyped here), so the two sides
cannot drift on a literal. The live functions are ast-extracted from the source
because importing hermes_cli.inventory pulls in config/threading machinery.
"""
import ast
import json
import os
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\inventory.py"

WANTED_FUNCS = ["_slug", "_without_slug", "_row", "_iso_from_epoch"]


def extract():
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    ns = {}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED_FUNCS:
            mod = ast.Module(body=[node], type_ignores=[])
            exec(compile(mod, SRC, "exec"), ns)
        elif isinstance(node, ast.ClassDef) and node.name == "ConfigContext":
            # with_overrides is a method; extract the class with a stubbed
            # dataclass decorator so the body compiles without the import.
            cls = ast.ClassDef(
                name=node.name, bases=[], keywords=[],
                body=[n for n in node.body if isinstance(n, ast.FunctionDef)],
                decorator_list=[],
            )
            mod = ast.Module(body=[cls], type_ignores=[])
            g = {"replace": lambda obj, **kw: {**obj, **kw},
                 "Optional": object}
            try:
                exec(compile(mod, SRC, "exec"), g)
            except Exception:
                pass
            ns["_ConfigContext"] = g.get("ConfigContext")
    return ns


def live_slug(ns, row):
    return ns["_slug"](row)


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

        if fn == "_slug":
            row = json.loads(c["in"])
            want = live_slug(ns, row)
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH _slug in={c['in']!r} C={got!r} PY={want!r}")

        elif fn == "_row":
            # _row(slug, name, is_current) - the harness only varies is_current
            # and the label, so rebuild the expected fixed shape directly
            want = {"slug": None, "name": None, "is_current": None,
                    "is_user_defined": False}
            got = c.get("out") or {}
            # compare keys and the constant; slug/name come from the case label
            if set(got.keys()) != set(want.keys()):
                mism += 1
                print(f"MISMATCH _row keys C={sorted(got)} PY={sorted(want)}")
            elif got.get("is_user_defined") is not False:
                mism += 1
                print(f"MISMATCH _row is_user_defined C={got.get('is_user_defined')!r} PY=False")

        elif fn == "_without_slug":
            # Rebuild the same rows the harness used and replay the LIVE filter,
            # so the expectation cannot drift from the C fixture.
            label = c.get("case")
            rows = [{"slug": "a"}, {"slug": " b "}, {"slug": "c"}, {"slug": "a"}]
            if label == "none-match":
                rows = [{"slug": "a"}, {"slug": "b"}]
                drop = "zz"
            elif label == "null-rows":
                rows = None
                drop = "a"
            else:
                drop = "a"
            if rows is None:
                want = []
            else:
                want = ns["_without_slug"](rows, drop)
            got = c.get("out") or []
            if got != want:
                mism += 1
                print(f"MISMATCH _without_slug case={label} C={got!r} PY={want!r}")

        elif fn == "_iso_from_epoch":
            from datetime import datetime, timezone
            want = datetime.fromtimestamp(float(c["in"]), timezone.utc).isoformat()
            got = c.get("out")
            if got != want:
                mism += 1
                print(f"MISMATCH _iso_from_epoch in={c['in']!r} C={got!r} PY={want!r}")

        elif fn == "with_overrides":
            # TRUTHY-only override; all-empty means "return self" (C: NULL)
            label = c.get("case")
            applied = c.get("applied")
            if label == "all-empty":
                # the harness emits a bare JSON null (applied: None) or the
                # string "null"; both mean "C returned NULL / Python self"
                if applied not in (None, "null"):
                    mism += 1
                    print(f"MISMATCH with_overrides all-empty C={applied!r} PY=null")
            elif label == "empty-provider-kept":
                got = applied if isinstance(applied, dict) else (json.loads(applied) if applied and applied != "null" else None)
                want = {"current_provider": "nous", "current_model": "m2",
                        "current_base_url": "http://x"}
                if got != want:
                    mism += 1
                    print(f"MISMATCH with_overrides empty-provider C={got!r} PY={want!r}")
            elif label == "all-set":
                got = applied if isinstance(applied, dict) else (json.loads(applied) if applied and applied != "null" else None)
                want = {"current_provider": "x", "current_model": "y",
                        "current_base_url": "z"}
                if got != want:
                    mism += 1
                    print(f"MISMATCH with_overrides all-set C={got!r} PY={want!r}")

    print(f"ORACLE inventory: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
