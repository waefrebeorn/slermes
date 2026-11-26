#!/usr/bin/env python3
"""audit_gate_shims.py — find gates that answer for a gate-local shim, not the port.

Bug class this exists for
-------------------------
A watercup gate is only evidence when the number it prints came out of the PORT.
Two shapes break that promise and both look green:

  FAKE      harness.c calls a symbol the port never defines, and shims.c
            (a gate-local reimplementation) supplies it. The gate measures the
            shim. Forever. Whatever the port does.
  SHADOWED  harness.c calls a symbol the port DOES define, but shims.c also
            defines it and lands on the link line FIRST, with
            -Wl,--allow-multiple-definition, so the shim wins.

Signal for both: the symbol the harness calls is defined in the gate's own
shim, so the port object is never reached.

Output classes per shim gate:
  FAKE      harness symbol defined only by shims.c       -> fix the harness
  SHADOWED  shim defines a symbol the port also defines   -> drop the shim
  DEAD      no shim symbol is called by the harness       -> delete shims.c

Exit: 0 when no FAKE/SHADOWED gate remains, 1 otherwise (fail closed).
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
GATES = REPO / "tests" / "watercup" / "gates"
BUILD = REPO / "tests" / "watercup" / "build"
ARCHIVE = BUILD / "_all_objects.a"

C_KEYWORDS = {
    "if", "for", "while", "switch", "return", "sizeof", "do", "else",
    "printf", "fprintf", "snprintf", "sprintf", "memset", "memcpy",
    "strlen", "strcmp", "strncmp", "strcpy", "strncpy", "strtok_r",
    "atoi", "atof", "strtod", "malloc", "calloc", "realloc", "free",
    "WCUP_BEGIN", "WCUP_END", "WCUP_VALSTR", "WCUP_VALINT", "WCUP_VALDOUBLE",
    "WCUP_FINALIZE", "WCUP_BRANCH", "main", "usage", "wcup",
    "__attribute__", "defined", "static_cast",
}

DEF_RE = re.compile(
    r"^[A-Za-z_][\w\s\*]*?\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{", re.MULTILINE
)
CALL_RE = re.compile(r"\b([A-Za-z_]\w*)\s*\(")


def nm_archive_symbols() -> set[str]:
    """Defined-and-exported symbols in the prebuilt object archive (rebuilt if stale)."""
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from _gate_audit_common import port_symbols
    syms = port_symbols()
    if not syms:
        raise SystemExit("no port symbols -- build the port first")
    return syms


def shim_defs(path: Path) -> set[str]:
    txt = path.read_text(errors="ignore")
    return {m.group(1) for m in DEF_RE.finditer(txt)} - C_KEYWORDS


def harness_calls(path: Path) -> set[str]:
    txt = path.read_text(errors="ignore")
    # drop comments so a symbol named in prose is not a call
    txt = re.sub(r"/\*.*?\*/", " ", txt, flags=re.S)
    txt = re.sub(r"//[^\n]*", " ", txt)
    return {m.group(1) for m in CALL_RE.finditer(txt)} - C_KEYWORDS


def port_defines(sym: str, archive: set[str], src_index: dict[str, list[str]],
                 skip: Path) -> bool:
    if sym in archive:
        return True
    return bool(src_index.get(sym))


def build_source_index() -> dict[str, list[str]]:
    """sym -> files under src/ and lib/ that look like they define it."""
    idx: dict[str, list[str]] = {}
    roots = [REPO / "src", REPO / "lib"]
    for root in roots:
        for p in root.rglob("*.c"):
            try:
                txt = p.read_text(errors="ignore")
            except OSError:
                continue
            for m in DEF_RE.finditer(txt):
                idx.setdefault(m.group(1), []).append(str(p.relative_to(REPO)))
    return idx


def main() -> int:
    archive = nm_archive_symbols()
    src_index = build_source_index()
    shim_gates = sorted(GATES.glob("*/shims.c"))
    rows = []
    for shim in shim_gates:
        gdir = shim.parent
        gate = json.loads((gdir / "gate.json").read_text())
        extra = [os.path.basename(e) for e in gate.get("extra_src", [])]
        on_link = "shims.c" in extra
        syms = shim_defs(shim)
        calls = harness_calls(gdir / "harness.c")
        used = syms & calls
        verdict, detail = "DEAD", []
        if used and on_link:
            # The shim is compiled AND linked ahead of the prebuilt archive, so whatever it
            # defines wins. Which symbol the port also defines decides the name of the lie:
            # FAKE     the port has no such symbol -> the gate can only measure the shim.
            # SHADOWED the port has it -> the port object is never reached.
            shadowed = [s for s in used if port_defines(s, archive, src_index, gdir)]
            verdict = "SHADOWED" if shadowed else "FAKE"
            for s in sorted(used):
                where = src_index.get(s, [])
                detail.append("%s%s" % (s, " (port: %s)" % where[0] if where else " (port: MISSING)"))
        rows.append({
            "gate": gate["name"], "verdict": verdict, "on_link": on_link,
            "shim_syms": sorted(syms), "used": sorted(used), "detail": detail,
        })

    bad = [r for r in rows if r["verdict"] in ("FAKE", "SHADOWED")]
    dead = [r for r in rows if r["verdict"] == "DEAD"]
    for r in bad:
        print("%-9s %-52s on_link=%s" % (r["verdict"], r["gate"], r["on_link"]))
        for d in r["detail"]:
            print("            %s" % d)
    for r in dead:
        print("%-9s %-52s on_link=%s (shim: %s)"
              % ("DEAD", r["gate"], r["on_link"], ",".join(r["shim_syms"]) or "-"))
    print("\n%d shim gates: %d FAKE/SHADOWED, %d DEAD, %d honest"
          % (len(rows), len(bad), len(dead), len(rows) - len(bad) - len(dead)))
    out = BUILD / "gate_shim_audit.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(rows, indent=1))
    print("wrote %s" % out.relative_to(REPO))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
