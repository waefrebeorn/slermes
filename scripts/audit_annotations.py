#!/usr/bin/env python3
"""audit_annotations.py — verify every PoP annotation credits the module that
actually DEFINES the function.

A `/* PoP: <cfunc> @ <module>:<func> */` annotation whose <module> does not
define <func> hides real coverage: the defining module reads as uncovered and
the next port target is chosen from a fiction. This was not hypothetical —
4,054 of 19,656 annotations credited a re-exporting module.

Guards that make --fix safe:
  * skip tests/, skills/, scripts/, optional-skills/, test_* and the nested
    slermes/ checkout;
  * the C name must be a valid identifier;
  * only re-point when exactly ONE reference module defines that name
    (generic names like join/fetch/search appear in several — left alone);
  * line endings are preserved (read_bytes/write_bytes).

Usage:
  python scripts/audit_annotations.py                 # report
  python scripts/audit_annotations.py --fix           # re-point the safe ones
  python scripts/audit_annotations.py --json
"""
import argparse
import ast
import glob
import io
import json
import os
import re
import sys
from collections import defaultdict

HERMES = "C:/Users/eman5/hermes-agent"
REPO = "C:/Users/eman5/slermes"
SKIP = ("tests/", "optional-skills/", "skills/", "scripts/")
ANN = re.compile(r"(PoP:\s*)(\S+)(\s*@\s*)([\w\./]+)(:)(\w+)")
IDENT = re.compile(r"[A-Za-z_]\w*$")


def load_reference():
    defs = {}
    for p in glob.glob(HERMES + "/**/*.py", recursive=True):
        rel = os.path.relpath(p, HERMES).replace("\\", "/")
        if rel.startswith(SKIP) or rel.startswith("slermes/"):
            continue
        if os.path.basename(p).startswith("test_"):
            continue
        try:
            tree = ast.parse(io.open(p, encoding="utf-8", errors="replace").read())
        except (SyntaxError, PermissionError, OSError):
            continue
        names = {n.name for n in ast.walk(tree)
                 if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))}
        if names:
            defs[rel] = names
    return defs


def scan(defs, fix=False):
    total = correct = repointed = 0
    miscredited = defaultdict(int)
    ambiguous = no_candidate = 0
    changed_files = 0

    for p in sorted(glob.glob(REPO + "/src/**/*.c", recursive=True)):
        raw = io.open(p, "rb").read()
        text = raw.decode("utf-8", errors="replace")
        file_changed = 0

        def repl(m):
            nonlocal file_changed
            prefix, cfunc, at, mod, colon, pyfunc = m.groups()
            if not IDENT.fullmatch(cfunc) or not mod.endswith(".py"):
                return m.group(0)
            if mod in defs and pyfunc in defs[mod]:
                return m.group(0)
            cands = sorted(k for k, v in defs.items() if pyfunc in v)
            if len(cands) != 1:
                return m.group(0)
            file_changed += 1
            return f"{prefix}{cfunc}{at}{cands[0]}{colon}{pyfunc}"

        for m in ANN.finditer(text):
            total += 1
            cfunc, mod, pyfunc = m.group(2), m.group(4), m.group(6)
            if not mod.endswith(".py"):
                continue
            if mod in defs and pyfunc in defs[mod]:
                correct += 1
                continue
            cands = sorted(k for k, v in defs.items() if pyfunc in v)
            if len(cands) == 1:
                miscredited[(mod, cands[0])] += 1
            elif len(cands) > 1:
                ambiguous += 1
            else:
                no_candidate += 1

        if fix and file_changed:
            new = ANN.sub(repl, text)
            if new != text:
                io.open(p, "wb").write(new.encode("utf-8"))
                repointed += file_changed
                changed_files += 1

    return {
        "total": total, "correct": correct, "repointed": repointed,
        "ambiguous": ambiguous, "no_candidate": no_candidate,
        "changed_files": changed_files,
        "miscredited": sorted(((v, k) for k, v in miscredited.items()),
                              reverse=True),
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--fix", action="store_true")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    defs = load_reference()
    res = scan(defs, fix=args.fix)

    if args.json:
        print(json.dumps(res, indent=2, default=str))
        return 0

    print(f"annotations:            {res['total']}")
    print(f"correct:                {res['correct']}")
    print(f"mis-credited (unique):  "
          f"{sum(v for v, _ in res['miscredited'])}")
    print(f"ambiguous (left alone): {res['ambiguous']}")
    print(f"no candidate found:     {res['no_candidate']}")
    if args.fix:
        print(f"\nre-pointed: {res['repointed']} in {res['changed_files']} files")
        return 0
    if res["miscredited"]:
        print("\ntop mis-credited (annotated -> defining):")
        for count, (mod, actual) in res["miscredited"][:20]:
            print(f"  {count:5d}  {mod} -> {actual}")
        print("\nre-run with --fix to re-point the unique-candidate ones.")
    return 1 if res["miscredited"] else 0


if __name__ == "__main__":
    sys.exit(main())
