#!/usr/bin/env python3
"""Find JSON argument strings that are read AFTER the object they point into is freed.

The bug shape, seen three times today in unrelated modules:

    query = json_get_str(args, "query", NULL);   /* pointer INTO args */
    ...
    json_free(args);                              /* query now dangles */
    skill_search(query, ...);                     /* reads freed memory */

It never crashes. It returns a plausible answer -- an empty string, the previous result, or text that
happens to still be there -- which is why it survives review and why the tests pass.

This scans each function body: names bound from `json_get_str` / `json_object_get_string` / a raw
`->str_val` on the same object, then flags any use of those names on a line after `json_free(obj)`.

Usage: python scripts/scan_json_freed_use.py [paths...]   (default: src/tools src/agent src/cli)
Exit code 1 when findings exist, so it can gate.

It is a *candidate* finder, not a proof: it has no control flow, so a free inside an `if` that exits
elsewhere can still be reported. Every hit must be read before it is "fixed" -- three of the first six
were real, and one "fix" of mine (a duplicate free on the success path) crashed the server until the
duplicate was removed.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]

FREE_RE = re.compile(r"\bjson_free\s*\(\s*([A-Za-z_]\w*)\s*\)")
# `x = json_get_str(obj, ...)` / `x = json_object_get_string(obj, ...)`
BIND_RE = re.compile(
    r"\b([A-Za-z_]\w*)\s*=\s*(?:json_get_str|json_object_get_string)\s*\(\s*([A-Za-z_]\w*)")
# `const char *x = json_get_str(obj, ...)` or with other qualifiers
DECL_BIND_RE = re.compile(
    r"\b(?:const\s+)?char\s*\*\s*([A-Za-z_]\w*)\s*=\s*"
    r"(?:json_get_str|json_object_get_string)\s*\(\s*([A-Za-z_]\w*)")


def functions(text: str):
    """Crude but adequate: a top-level `{ ... }` block with a signature, braces balanced."""
    i = 0
    n = len(text)
    while i < n:
        brace = text.find("{", i)
        if brace == -1:
            return
        # find the start of the signature (previous line boundary)
        start = text.rfind("\n", 0, brace)
        sig = text[max(0, start - 400):brace]
        if ";" in sig.split(")")[-1] if ")" in sig else False:
            pass
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
        body = text[brace:j + 1]
        name = "?"
        for m in re.finditer(r"([A-Za-z_]\w*)\s*\(", sig):
            name = m.group(1)
        yield name, brace, body
        i = j + 1


def blank_strings(line: str) -> str:
    """Remove string-literal contents: `json_new_string("[saved to file]")` is not a use of a variable."""
    out = []
    i = 0
    in_str = False
    while i < len(line):
        c = line[i]
        if c == '"' and (i == 0 or line[i - 1] != "\\"):
            in_str = not in_str
            out.append(c)
        elif in_str:
            out.append(" ") if c != "\\" else out.append(c)
            if c == "\\" and i + 1 < len(line):
                out.append(" ")
                i += 1
        else:
            out.append(c)
        i += 1
    return "".join(out)


def strip_comments(line: str) -> str:
    out = []
    i = 0
    in_str = False
    while i < len(line):
        c = line[i]
        if c == '"' and (i == 0 or line[i - 1] != "\\"):
            in_str = not in_str
            out.append(c)
        elif not in_str and line.startswith("//", i):
            break
        elif not in_str and line.startswith("/*", i):
            end = line.find("*/", i + 2)
            if end == -1:
                break
            i = end + 2
            continue
        else:
            out.append(c)
        i += 1
    return "".join(out)


def main() -> int:
    roots = [Path(a) for a in sys.argv[1:]] or [REPO / "src/tools", REPO / "src/agent", REPO / "src/cli"]
    findings = []
    scanned = 0

    for root in roots:
        for path in sorted(root.rglob("*.c")):
            text = path.read_text(encoding="utf-8", errors="replace")
            # Whole-file block comments first: a `*` continuation line otherwise reads as code.
            text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
            scanned += 1
            file_start = 0
            while True:
                nb = text.find("\n", file_start)
                if nb == -1:
                    break
                brace_line = text[file_start:nb]
                file_start = nb + 1
                break

            for fname, offset, body in functions(text):
                lines = body.splitlines()
                base = text[:offset].count("\n") + 1
                # object -> (pointer name -> (line index of binding))
                bound: dict[str, list[tuple[str, int]]] = {}
                freed: dict[str, int] = {}
                for idx, raw in enumerate(lines):
                    line = blank_strings(strip_comments(raw))
                    hits = {}
                    for m in list(DECL_BIND_RE.finditer(line)) + list(BIND_RE.finditer(line)):
                        hits[(m.group(2), m.group(1))] = True   # dedupe declaration+plain match
                    for (obj, ptr) in hits:
                        bound.setdefault(obj, []).append((ptr, idx))
                    for m in FREE_RE.finditer(line):
                        obj_name = m.group(1)
                        tail = line[m.end():]
                        # A free on the way out of the function (or out of the block) is not the
                        # dangling kind: `if (!x) { json_free(args); return ...; }`.
                        terminal = "return" in tail or "break" in tail or "continue" in tail
                        if not terminal:
                            for nxt in range(idx + 1, min(idx + 7, len(lines))):
                                nxt_code = strip_comments(lines[nxt]).strip()
                                if not nxt_code or nxt_code.startswith(("free(", "//")):
                                    continue          # cleanup between the free and the exit
                                if nxt_code.startswith(("return", "break", "continue", "goto")):
                                    terminal = True
                                break
                        if not terminal:
                            freed.setdefault(obj_name, idx)

                seen: set[tuple[str, str, int]] = set()
                for obj, frees in freed.items():
                    if obj not in bound:
                        continue
                    for ptr, bind_idx in bound[obj]:
                        if (obj, ptr, bind_idx) in seen:
                            continue
                        seen.add((obj, ptr, bind_idx))
                        if ptr == obj or bind_idx >= frees:
                            continue
                        free_idx = frees
                        for use_idx in range(free_idx + 1, len(lines)):
                            code = blank_strings(strip_comments(lines[use_idx]))
                            # Not a struct field, and not a bare pointer-value test.
                            pat = rf"(?<![.>\w]){re.escape(ptr)}\b"
                            if not re.search(pat, code):
                                continue
                            bare_test = re.fullmatch(
                                rf"\s*(?:if|while)\s*\(\s*!?\s*{re.escape(ptr)}\s*\)\s*\{{?", code) or \
                                re.search(rf"\b{re.escape(ptr)}\s*(?:==|!=)\s*", code) or \
                                re.search(rf"!\s*{re.escape(ptr)}\b", code)
                            if bare_test and not re.search(rf"{re.escape(ptr)}\s*[\[\>]", code):
                                break
                            # assignment to it is fine; a dereference/read is not
                            if re.search(rf"\b{re.escape(ptr)}\s*=", code) and \
                               not re.search(rf"\b{re.escape(ptr)}\s*==", code):
                                break   # reassigned: no longer the freed pointer
                            findings.append((path, base + use_idx, fname, ptr, obj, base + free_idx,
                                             code.strip()[:120]))
                            break

    for path, line, fname, ptr, obj, free_line, code in findings:
        try:
            rel = path.resolve().relative_to(REPO)
        except ValueError:
            rel = path
        print(f"{rel}:{line}: `{ptr}` ({fname}) is used after json_free({obj}) at line {free_line}")
        print(f"    {code}")

    print(f"scanned {scanned} files, {len(findings)} use-after-free finding(s)")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
