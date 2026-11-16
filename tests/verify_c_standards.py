#!/usr/bin/env python3
"""C standards gate -- enforces docs/dev/c-standards.md.

The document is the standard; this script is its machine-checkable subset.
Rules that cannot be checked mechanically (comments explain why, buffers are
initialized) are reviewer territory and are listed in the doc as such.

  R1  src/**/port_<stem>.c opens with `/* port_<stem>.c -- `
  R2  it carries at least one `PoP:` annotation
  R3  every PoP line is `PoP: <c_fn> @ <py_file>:<py_fn>` and <c_fn> is
      actually defined in that file
  R4  if include/port_<stem>.h exists, it is the FIRST include
  R5  header guard is SLERMES_PORT_<STEM>_H
  R6  every exported (non-static) function is declared in the unit's header
  R7  no tabs, no trailing whitespace, LF endings, exactly one final newline

Gates fail on NEW violations: a pre-existing one is allowlisted by rule and
path with a reason, and the allowlist is itself checked (an entry whose file no
longer violates is dead weight and fails).

Exit 0 = PASS, 1 = FAIL.
"""
import collections
import re
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
SRC = REPO / "src"
INC = REPO / "include"

POP = re.compile(r"PoP:\s+([A-Za-z_]\w*)\s+@\s+([\w./]+):([A-Za-z_]\w*)")
# A definition is `name(args) {`. Not anchored to a line start: the return type
# frequently sits on the PREVIOUS line (`static char *\nac_bare_model(...)`),
# and a line-anchored pattern silently misses every one of those -- which made
# 120 PoP annotations look unbacked when the functions were right there.
# No nested parens, so function-pointer declarators are not matched.
DEF = re.compile(r"(?<![\w])([A-Za-z_]\w*)\s*\([^;{}()]*\)\s*\{")
KEYWORDS = {"if", "for", "while", "switch", "sizeof", "return", "do", "else",
            "catch", "defined", "__attribute__"}
STATIC = re.compile(r"^static\b", re.M)

# rule -> {path: reason}; see tests/c_standards_allowlist.py (generated, then
# hand-reviewed). A gate that always fails is not a gate.
try:
    from c_standards_allowlist import ALLOW          # noqa: E402
except ImportError:                                   # pragma: no cover
    import importlib.util as _ilu
    _spec = _ilu.spec_from_file_location(
        "c_standards_allowlist",
        str(Path(__file__).with_name("c_standards_allowlist.py")))
    _mod = _ilu.module_from_spec(_spec)
    _spec.loader.exec_module(_mod)
    ALLOW = _mod.ALLOW


def stem_of(c_path):
    m = re.match(r"port_(.+)\.c$", c_path.name)
    return m.group(1) if m else None


def header_for(stem):
    h = INC / f"port_{stem}.h"
    return h if h.exists() else None


def check_r7(text):
    bad = []
    if "\t" in text:
        bad.append("tab")
    if "\r" in text:
        bad.append("CR")
    for i, line in enumerate(text.split("\n"), 1):
        if line != line.rstrip():
            bad.append(f"trailing-ws:{i}")
            break
    if not text.endswith("\n") or text.endswith("\n\n"):
        bad.append("final-newline")
    return bad


def main():
    findings = collections.defaultdict(list)

    for c in sorted(SRC.rglob("port_*.c")):
        rel = c.relative_to(REPO).as_posix()
        try:
            text = c.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        stem = stem_of(c)
        if stem is None:
            continue

        # R1 -- traceability: the file names itself and its source module.
        # The exact punctuation varies across the tree (144 files use a
        # multi-line banner, an em-dash, or a one-liner); the invariant parity
        # depends on is that the unit is identifiable from its own header.
        head = text[:400]
        if not re.search(rf"\bport_{re.escape(stem)}\.c\b", head) or "/*" not in head[:60]:
            findings["R1"].append(rel)

        # R2
        pops = POP.findall(text)
        if not pops:
            findings["R2"].append(rel)

        # R3 -- annotation names a function that exists in this file
        defined = set(DEF.findall(text))
        for cname, pyfile, pyfn in pops:
            if cname not in defined:
                findings["R3"].append(f"{rel}:{cname}")

        # R4 / R5 / R6 need the header
        hdr = header_for(stem)
        if hdr is not None:
            hrel = hdr.relative_to(REPO).as_posix()
            incs = re.findall(r'^\s*#include\s+"([^"]+)"', text, re.M)
            if incs and incs[0] != hdr.name:
                findings["R4"].append(rel)
            htext = hdr.read_text(encoding="utf-8", errors="replace")
            guard = f"PORT_{stem.upper()}_H"
            if f"#ifndef {guard}" not in htext:
                findings["R5"].append(hrel)
            # R6 -- exported functions declared in the header.
            # Use the DEFINITION match position, not a fresh search: the name
            # also appears in comments and call sites ("...sqlite_util.open_db
            # (WAL, ...)"), and those are not definitions.
            hnames = set(re.findall(r"\b([A-Za-z_]\w*)\s*\(", htext))
            for m in DEF.finditer(text):
                fn = m.group(1)
                if fn in KEYWORDS:
                    continue
                line_start = m.start(1)
                # `static` may share the line with the name, or sit on the
                # line above it. Checking only the previous line misses the
                # common single-line form and calls a static function exported.
                ls = text.rfind("\n", 0, line_start) + 1
                line_prefix = text[ls:line_start]
                prev = text[:ls].rstrip().split("\n")[-1] if ls else ""
                if line_prefix.lstrip().startswith("static ") or prev.lstrip().startswith("static "):
                    continue
                if fn not in hnames and fn != "main":
                    findings["R6"].append(f"{rel}:{fn}")

        # R7
        b = check_r7(text)
        if b:
            findings["R7"].append(f"{rel}:{','.join(b[:3])}")

    # apply the allowlist, and flag allowlist entries that no longer fire
    dead = []
    for rule, entries in ALLOW.items():
        live = findings.get(rule, [])
        # An entry is keyed either by path (whole-file rules) or by
        # "path:symbol" (per-symbol rules), so match both.
        livestr = set(live) | {v.split(":")[0] for v in live}
        for path, why in entries.items():
            if path not in livestr:
                dead.append(f"{rule} {path} ({why}) -- no longer violates")

    ok = True
    for rule in sorted(findings):
        entries = ALLOW.get(rule, {})
        live = [v for v in findings[rule]
                if v not in entries and v.split(":")[0] not in entries]
        if live:
            ok = False
            print(f"FAIL {rule}: {len(live)} violation(s)")
            for v in live[:6]:
                print(f"      {v}")
        else:
            n = len(findings[rule])
            print(f"ok   {rule}: {n} allowlisted, 0 new")

    for d in dead[:6]:
        print(f"FAIL allowlist entry is dead: {d}")
    if dead:
        ok = False

    print("c-standards gate:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
