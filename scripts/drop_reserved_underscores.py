#!/usr/bin/env python3
"""Rename file-scope `_func` to `func`, per references/naming-strategy.md.

The strategy is explicit: "Python private functions (_func_name) drop the
underscore in C." These 1,782 identifiers are reserved by ISO C at file scope
(anything beginning with _ is reserved for the implementation), so they are a
portability hazard, not just style.

A name is SKIPPED when the de-underscored form already exists elsewhere in the
tree -- renaming it would create a duplicate symbol. Those are reported.

Whole-word, identifier-exact rewrite across src/ lib/ include/. Vendored trees
are excluded.
"""
import collections
import re
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
VEND = ("whisper_cpp", "ctranslate2_src", "stb_truetype", "libncurses",
        "libwubuoffice")
DEF = re.compile(r"^[A-Za-z_][\w\s\*]*\b(_[A-Za-z]\w*)\s*\(", re.M)
IDENT = re.compile(r"\b[A-Za-z_]\w*\b")


def sources():
    for base in ("src", "lib", "include"):
        for f in (REPO / base).rglob("*"):
            if f.suffix in (".c", ".h") and not any(v in f.as_posix() for v in VEND):
                yield f


def main() -> int:
    names = set()
    all_ids = collections.Counter()
    for f in sources():
        t = f.read_text(encoding="utf-8", errors="replace")
        names.update(m.group(1) for m in DEF.finditer(t))
        all_ids.update(IDENT.findall(t))

    skip = sorted(n for n in names if all_ids[n[1:]] > 0)
    rename = {n: n[1:] for n in names if all_ids[n[1:]] == 0}

    print(f"leading-underscore file-scope names: {len(names)}")
    print(f"  renamed (no collision)         : {len(rename)}")
    print(f"  SKIPPED (target already exists): {len(skip)}")
    for n in skip[:15]:
        print(f"    SKIP {n} -> {n[1:]} already present")

    changed = 0
    sites = 0
    for f in sources():
        t = f.read_bytes().decode("utf-8")
        orig = t

        def sub(m):
            nonlocal sites
            new = rename.get(m.group(0))
            if new:
                sites += 1
                return new
            return m.group(0)

        t = re.sub(r"\b_[A-Za-z]\w*\b", sub, t)
        if t != orig:
            f.write_bytes(t.encode("utf-8"))
            changed += 1
    print(f"rewrote {sites} identifier sites in {changed} files")

    # verify: no leading-underscore definition remains among renamed names
    left = collections.Counter()
    for f in sources():
        t = f.read_text(encoding="utf-8", errors="replace")
        for m in DEF.finditer(t):
            if m.group(1) in rename:
                left[m.group(1)] += 1
    print(f"renamed names still defined with a leading underscore: "
          f"{sum(left.values())}")
    return 1 if left else 0


if __name__ == "__main__":
    sys.exit(main())
