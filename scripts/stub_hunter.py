#!/usr/bin/env python3
"""scripts/stub_hunter.py — project-wide bootleg/stub detector (v2.1).

Scans every src/**/*.c function body and classifies the "no water" class:
  EMPTY_BODY   — function compiles but does nothing
  CONST_RETURN — body only returns a constant (NULL/0/1/-1/""/true/false)

Linear-time: one pass per file, no backtracking-prone regexes over whole
files (v2's `(?:\\w+\\s*\\*?\\s*)+` was catastrophic on minified lines).
Function heads are found by brace-depth tracking instead.

Usage:
  python3 scripts/stub_hunter.py [--json] [--quiet] [--root src]
"""
import argparse
import json
import re
import sys
from pathlib import Path

SKIP_NAMES = {
    "if", "while", "for", "switch", "return", "sizeof", "else", "do",
    "defined",
}
CONST_RET = re.compile(
    r"return\s+(?:NULL|0|1|-1|0\.0|\"\"|true|false"
    r"|json_null\s*\(\s*\)|json_new_null\s*\(\s*\))$")
VOID_CAST = re.compile(r"\(\s*void\s*\)\s*\w+$")
CTRL_KW = re.compile(r"\b(if|for|while|switch)\b")


def strip_comments(text):
    """Linear comment stripper."""
    out = []
    i = 0
    n = len(text)
    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if ch == "/" and nxt == "*":
            end = text.find("*/", i + 2)
            i = (end + 2) if end != -1 else n
            out.append(" ")
        elif ch == "/" and nxt == "/":
            end = text.find("\n", i)
            i = n if end == -1 else end
        elif ch == '"':
            j = i + 1
            while j < n:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == '"':
                    break
                j += 1
            out.append(text[i:j + 1])
            i = j + 1
        else:
            out.append(ch)
            i += 1
    return "".join(out)


def classify_body(body):
    bs = body[1:-1].strip()
    if not bs:
        return "EMPTY_BODY"
    # any control keyword or nested braces => real work
    if CTRL_KW.search(bs) or "{" in bs[1:-1]:
        return None
    stmts = [x.strip() for x in bs.split(";") if x.strip()]
    if not stmts:
        return "EMPTY_BODY"
    for st in stmts:
        if CONST_RET.fullmatch(st):
            continue
        if VOID_CAST.fullmatch(st):
            continue
        return None
    return "CONST_RETURN"


def scan(root="src"):
    results = []
    total = 0
    for p in sorted(Path(root).rglob("*.c")):
        try:
            raw = p.read_text(errors="replace")
        except OSError:
            continue
        nc = strip_comments(raw)

        depth = 0
        head_start = -1   # start of the current top-level statement chunk
        i = 0
        n = len(nc)
        fn_start = -1     # index of '{' opening a function body
        name = None

        while i < n:
            ch = nc[i]
            if ch == "{":
                if depth == 0:
                    # candidate: look backward for identifier( ... ) pattern
                    seg = nc[max(0, i - 600):i]
                    m = None
                    for mm in re.finditer(r"([A-Za-z_]\w*)\s*\(", seg):
                        m = mm
                    if m:
                        cand = m.group(1)
                        tail = seg[m.start():]
                        # reject control keywords and struct/enum initializers
                        if cand not in SKIP_NAMES and "=" not in tail.split("(")[0][-60:]:
                            name = cand
                            fn_start = i
                            total += 1
                            body_end = _find_close(nc, i)
                            if body_end is not None:
                                kind = classify_body(nc[i:body_end + 1])
                                if kind:
                                    results.append({"file": str(p),
                                                    "fn": name,
                                                    "kind": kind})
                                i = body_end + 1
                                name = None
                                continue
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth < 0:
                    depth = 0
            i += 1
    return total, results


def _find_close(nc, open_idx):
    depth = 0
    i = open_idx
    in_str = False
    n = len(nc)
    while i < n:
        ch = nc[i]
        if in_str:
            if ch == "\\":
                i += 2
                continue
            if ch == '"':
                in_str = False
        elif ch == '"':
            in_str = True
        elif ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return None


def main():
    ap = argparse.ArgumentParser(description="bootleg/stub detector")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--root", default="src")
    args = ap.parse_args()

    total, results = scan(args.root)
    if args.json:
        print(json.dumps({"total_functions": total, "suspicious": results},
                         indent=1))
        return 0
    print(f"functions scanned: {total}")
    print(f"suspicious (empty/const-return bodies): {len(results)}")
    by_kind = {}
    for r in results:
        by_kind.setdefault(r["kind"], []).append(r)
    for kind in sorted(by_kind):
        print(f"\n== {kind} ({len(by_kind[kind])}) ==")
        for r in by_kind[kind]:
            print(f"  {r['file']}: {r['fn']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
