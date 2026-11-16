#!/usr/bin/env python3
"""Add missing function declarations to an oracle harness, using the exact
signature from the first-party definition.

Better than hand-writing prototypes: a hand-written one with the wrong
parameter type produces `conflicting types` at the call site, and the right
answer is whatever the real definition says.

Usage: fix_harness_protos.py <logfile>...
"""
import re
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")

# A definition line: return type + name + parameter list, ending in ')'.
DEF = re.compile(
    r"^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\(([^;{]*)\)\s*\{",
    re.M)
MISSING = re.compile(
    r"implicit declaration of function '([A-Za-z_]\w*)'")
CONFLICT = re.compile(
    r"conflicting types for '([A-Za-z_]\w*)'")


def definitions():
    """symbol -> exact prototype, from the first-party tree."""
    out = {}
    for base in ("src", "lib", "include"):
        for f in (REPO / base).rglob("*"):
            if f.suffix not in (".c", ".h"):
                continue
            if "whisper_cpp" in f.as_posix() or "ctranslate2_src" in f.as_posix():
                continue
            try:
                txt = f.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            for m in DEF.finditer(txt):
                name = m.group(1)
                params = " ".join(m.group(2).split())
                if params == "":
                    params = "void"
                ret = m.group(0)[:m.start(1)]
                ret = re.sub(r"\b" + re.escape(name) + r"\s*\(.*$", "", ret,
                             flags=re.S).strip()
                ret = " ".join(ret.split())
                proto = f"{ret} {name}({params});"
                out.setdefault(name, proto)
    return out


def main(logs):
    defs = definitions()
    total = 0
    for log in logs:
        text = Path(log).read_text(encoding="utf-8", errors="replace")
        m = re.search(r"(tests/t_port_\w+\.c)", text)
        if not m:
            continue
        f = REPO / m.group(1)
        if not f.exists():
            continue
        src = f.read_text(encoding="utf-8")
        missing = set(MISSING.findall(text)) | set(CONFLICT.findall(text))
        added = []
        for name in sorted(missing):
            proto = defs.get(name)
            if not proto:
                continue
            # Identical redeclaration is legal in C, so it is enough to check
            # whether this exact prototype is already present. Do NOT test for
            # a loose `name(...);` -- that also matches call sites like
            # `x = json_deep_copy(y);` and suppresses a needed prototype.
            if proto in src:
                continue
            lines = src.split("\n")
            idx = max(i for i, l in enumerate(lines) if l.startswith("#include"))
            lines.insert(idx + 1, "/* declared here: the harness's includes do not reach it */")
            lines.insert(idx + 2, proto)
            src = "\n".join(lines)
            added.append(proto)
        if added:
            f.write_text(src, encoding="utf-8")
            total += len(added)
            print(f"  {f.name}: {len(added)} prototype(s)")
            for p in added:
                print(f"      {p}")
    print("prototypes added:", total)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
