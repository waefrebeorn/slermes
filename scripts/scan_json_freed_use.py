#!/usr/bin/env python3
"""Find JSON argument strings that are read AFTER the object they point into is freed.

The bug shape, seen four times in unrelated modules:

    query = json_get_str(args, "query", NULL);   /* pointer INTO args */
    ...
    json_free(args);                              /* query now dangles */
    skill_search(query, ...);                     /* reads freed memory */

It never crashes. It returns a plausible answer -- an empty string, the previous result, or text that
happens to still be there -- which is why it survives review and why the tests pass.

A name bound from `json_get_str` / `json_object_get_string` is tracked until the object it came from is
`json_free`d; a *dereference* of that name after the free, before control leaves the block the free sits
in, is reported.

This is a heuristic, not a proof. It has no alias tracking and no full control flow: a free inside an
`if` whose every path exits elsewhere can still be reported, and a pointer that was copied to a safe
string can still be flagged. Read every hit before "fixing" it -- and run
tests/unit/scan_json_freed_use_selftest.py after touching this file, because twice it has been quietly
turned into a scanner that misses the real shape.

Usage: python scripts/scan_json_freed_use.py [paths...]   (default: src/tools src/agent src/cli)
Exit code 1 when findings exist.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]

FREE_RE = re.compile(r"\bjson_free\s*\(\s*([A-Za-z_]\w*)\s*\)")
BIND_RE = re.compile(
    r"\b([A-Za-z_]\w*)\s*=\s*"
    r"(?:json_get_str|json_object_get_string)\s*\(\s*([A-Za-z_]\w*)")
DEREF_CALLS = ("strlen", "strcmp", "strncmp", "strcpy", "strncpy", "strcat", "strdup",
               "strstr", "strchr", "snprintf", "printf", "fprintf", "fopen", "stat",
               "url_has_secret", "url_is_safe", "free")


def blank_strings(line: str) -> str:
    """Blank string-literal contents: json_new_string("path") is not a use of `path`."""
    out = []
    in_str = False
    i = 0
    while i < len(line):
        c = line[i]
        if in_str:
            if c == "\\" and i + 1 < len(line):
                out.append("  ")
                i += 2
                continue
            if c == '"':
                in_str = False
                out.append(c)
            else:
                out.append(" ")
        else:
            if c == '"':
                in_str = True
            out.append(c)
        i += 1
    return "".join(out)


def strip_comments(line: str) -> str:
    out = []
    i = 0
    in_str = False
    while i < len(line):
        c = line[i]
        if in_str:
            out.append(c)
            if c == "\\" and i + 1 < len(line):
                out.append(line[i + 1])
                i += 2
                continue
            if c == '"':
                in_str = False
        elif c == '"':
            in_str = True
            out.append(c)
        elif line.startswith("//", i):
            break
        elif line.startswith("/*", i):
            end = line.find("*/", i + 2)
            if end == -1:
                break
            i = end + 2
            continue
        else:
            out.append(c)
        i += 1
    return "".join(out)


def clean(line: str) -> str:
    return blank_strings(strip_comments(line))


def functions(text: str):
    i = 0
    n = len(text)
    while True:
        brace = text.find("{", i)
        if brace == -1:
            return
        depth = 0
        j = brace
        while j < n:
            if text[j] == "{":
                depth += 1
            elif text[j] == "}":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        sig_start = max(0, text.rfind("\n", 0, brace) - 300)
        sig = text[sig_start:brace]
        name = "?"
        for m in re.finditer(r"([A-Za-z_]\w*)\s*\(", sig):
            name = m.group(1)
        yield name, brace, text[brace:j + 1]
        i = j + 1


def depths_of(lines):
    depths = []
    d = 0
    for line in lines:
        depths.append(d)
        code = clean(line)
        d += code.count("{") - code.count("}")
    return depths


def uses(ptr: str, code: str) -> bool:
    return bool(re.search(rf"(?<![.>\w]){re.escape(ptr)}\b", code))


def dereferences(ptr: str, code: str) -> bool:
    p = re.escape(ptr)
    if re.search(rf"(?<![.>\w]){p}\s*\[", code):        # x[i]
        return True
    if re.search(rf"(?<![.>\w]){p}\s*->", code):        # x->field
        return True
    if re.search(rf"(?<![.>\w])\*{p}\b", code):         # *x
        return True
    if re.search(rf"\b(?:{'|'.join(DEREF_CALLS)})\s*\([^;]*\b{p}\b", code):
        return True                                      # strlen(x), fopen(x, ...)
    if re.search(rf"\w+\s*\([^;]*\b{p}\b", code):     # passed into any call
        return True
    if re.search(rf"(?<![.>\w]){p}\s*,", code):          # first of several arguments
        return True
    if re.search(rf",\s*{p}\s*(?:\)|,)", code):
        return True
    return False


def scan_file(path: Path):
    raw = path.read_text(encoding="utf-8", errors="replace")
    raw = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), raw, flags=re.S)

    findings = []
    for fname, offset, body in functions(raw):
        lines = body.splitlines()
        if not lines:
            continue
        depths = depths_of(lines)
        base = raw[:offset].count("\n") + 1

        bound = {}
        free_sites = {}
        for idx, line0 in enumerate(lines):
            code = clean(line0)
            for m in BIND_RE.finditer(code):
                bound.setdefault(m.group(2), []).append((m.group(1), idx))
            for m in FREE_RE.finditer(code):
                tail = code[m.end():]
                if any(k in tail for k in ("return", "break", "continue", "goto")):
                    continue                      # ordinary free on the way out
                free_sites.setdefault(m.group(1), []).append((idx, depths[idx]))

        seen = set()
        for obj, sites in free_sites.items():
            for ptr, bind_idx in bound.get(obj, []):
                if ptr == obj or (obj, ptr, bind_idx) in seen:
                    continue
                seen.add((obj, ptr, bind_idx))
                for free_idx, free_depth in sites:
                    if bind_idx >= free_idx:
                        continue
                    hit = None
                    for use_idx in range(free_idx + 1, len(lines)):
                        code = clean(lines[use_idx])
                        # Leaving the block does not make the pointer valid again; only a
                        # return/break/continue at this depth ends the flow.
                        if uses(ptr, code):
                            if re.search(rf"(?<![.>\w]){re.escape(ptr)}\s*=(?!=)", code):
                                break             # rebound: no longer the freed pointer
                            if dereferences(ptr, code):
                                hit = use_idx
                                break
                        if depths[use_idx] == free_depth and re.search(
                                r"\b(return|break|continue|goto)\b", code):
                            break                 # flow ended at the free's own level
                    if hit is not None:
                        findings.append((base + hit, fname, ptr, obj, base + free_idx,
                                         clean(lines[hit]).strip()[:120]))
                        break
    return findings


def main() -> int:
    roots = [Path(a) for a in sys.argv[1:]] or [REPO / "src/tools", REPO / "src/agent", REPO / "src/cli"]
    findings = 0
    scanned = 0
    for root in roots:
        paths = [root] if root.is_file() else sorted(root.rglob("*.c"))
        for path in paths:
            scanned += 1
            for line, fname, ptr, obj, free_line, code in scan_file(path):
                try:
                    rel = path.resolve().relative_to(REPO)
                except ValueError:
                    rel = path
                print(f"{rel}:{line}: `{ptr}` ({fname}) dereferenced after json_free({obj}) at line {free_line}")
                print(f"    {code}")
                findings += 1
    print(f"scanned {scanned} files, {findings} candidate(s)")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
