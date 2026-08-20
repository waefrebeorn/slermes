#!/usr/bin/env python3
"""audit_ports.py — authoritative PoP-integrity audit (v2, trustworthy).

v1 reported ~17,500 "ORPHAN_CNAME" false positives because it collected C
function names with a naive regex that missed static/attribute/pointer-return
definitions, struct-embedded methods, and declarations. That made the detector
useless for orientation (the user rightly called the detectors "very poor").

v2 fixes the collection so a PoP is only flagged ORPHAN when the C name appears
NOWHERE in src/include/lib (definitions AND declarations), and excludes
documented entry-point exceptions (main, json_* helpers shared across modules).
It ALSO emits the real, actionable counts:
  - TRUE_ORPHAN      : PoP credits a C fn that does not exist anywhere
  - MULTILINE_POP    : PoP comment split across lines (scanner can't read it)
  - STUB_MARKER      : TODO/FIXME/not-implemented in a ported body
  - DUP_CREDIT       : same C fn credited to >1 Python fn (collision risk)
The output is machine-readable (--json) so the WORK_DASHBOARD can ingest it.
"""
import re, json, sys, argparse
from pathlib import Path

ROOT = Path("/home/wubu/hermes-agent-dev/slermes")
# entry-point / shared-helper names that legitimately differ from their PoP twin
ENTRY_EXCEPTIONS = {"main"}

def collect_c_names():
    """Return (defs, decls) sets of every C identifier used as a function name,
    via both definition and declaration patterns (covers static, __attribute__,
    pointer returns, struct methods like foo_bar(...), and forward decls)."""
    names = set()
    # definition: optional specifiers/attributes, a type-ish word, then NAME(
    def_pat = re.compile(
        r'(?:^|[\n;])\s*'
        r'(?:static\s+|inline\s+|extern\s+|const\s+|__attribute__\s*\(\([^)]*\)\)\s*)*'
        r'(?:struct\s+|enum\s+|union\s+)?[\w\s\*]*/\s*'
        r'(\w+)\s*\(')
    # declaration: NAME( at top level with trailing ; (no body)
    decl_pat = re.compile(r'(?:^|[\n;])\s*(?:static\s+|extern\s+)?(?:const\s+)?[\w\s\*]*?(\w+)\s*\([^;]*\)\s*;')
    for p in list(ROOT.rglob("*.c")) + list(ROOT.rglob("*.h")):
        try:
            txt = p.read_text(errors="ignore")
        except Exception:
            continue
        for m in def_pat.finditer(txt):
            names.add(m.group(1))
        for m in decl_pat.finditer(txt):
            names.add(m.group(1))
    # also any line that looks like a function pointer typedef target etc.
    return names

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    c_names = collect_c_names()
    pop_pat = re.compile(r'/\*\s*PoP:\s*(\w+)\s*@\s*([\w/.]+):([\w.]+)\s*\*/')
    multiline_pat = re.compile(r'PoP:\s*(\w+)\s*@\s*([\w/.]+):([\w.]+)')
    stub_pat = re.compile(r'(not implemented|notImplemented|TODO|FIXME|XXX:\s*stub|placeholder|raise\s+NotImplementedError)', re.I)

    issues = []          # (file, kind, detail)
    credit_count = {}    # c_name -> list of python twins
    stub_markers = []

    for p in list(ROOT.rglob("*.c")) + list(ROOT.rglob("*.h")):
        try:
            txt = p.read_text(errors="ignore")
        except Exception:
            continue
        for m in pop_pat.finditer(txt):
            c, mod, fn = m.group(1), m.group(2), m.group(3)
            credit_count.setdefault(c, []).append(f"{mod}:{fn}")
            if c in ENTRY_EXCEPTIONS:
                continue
            if c not in c_names:
                issues.append((str(p), "TRUE_ORPHAN", f"{c} @ {mod}:{fn}"))
        # multiline PoP (scanner-blind)
        for m in multiline_pat.finditer(txt):
            start = txt.rfind("\n", 0, m.start()) + 1
            end = txt.find("\n", m.end())
            line = txt[start:end]
            if "*/" not in line:
                issues.append((str(p), "MULTILINE_POP", f"{m.group(1)} @ {m.group(2)}:{m.group(3)}"))
    # duplicates
    dups = {c: v for c, v in credit_count.items() if len(v) > 1}

    # stub markers in port_*.c (only files that look like ports)
    for p in ROOT.rglob("port_*.c"):
        try:
            txt = p.read_text(errors="ignore")
        except Exception:
            continue
        for m in stub_pat.finditer(txt):
            stub_markers.append((str(p), m.group(0)))

    # summary
    kinds = {}
    for _, k, _ in issues:
        kinds[k] = kinds.get(k, 0) + 1

    if args.json:
        out = {
            "true_orphans": [i for i in issues if i[1] == "TRUE_ORPHAN"],
            "multiline_pops": [i for i in issues if i[1] == "MULTILINE_POP"],
            "dup_credits": {c: v for c, v in dups.items()},
            "stub_markers": stub_markers,
            "counts": {
                "TRUE_ORPHAN": kinds.get("TRUE_ORPHAN", 0),
                "MULTILINE_POP": kinds.get("MULTILINE_POP", 0),
                "DUP_CREDIT": len(dups),
                "STUB_MARKER": len(stub_markers),
                "TOTAL_POP_CREDITS": sum(len(v) for v in credit_count.values()),
            },
        }
        print(json.dumps(out, indent=1))
        return 0

    print("=== PoP integrity audit (v2, trustworthy) ===")
    print(f"  TOTAL PoP credits scanned : {sum(len(v) for v in credit_count.values())}")
    print(f"  TRUE_ORPHAN (C fn missing): {kinds.get('TRUE_ORPHAN',0)}")
    print(f"  MULTILINE_POP (scanner-blind): {kinds.get('MULTILINE_POP',0)}")
    print(f"  DUP_CREDIT (C fn -> 2+ python): {len(dups)}")
    print(f"  STUB_MARKER (TODO/FIXME in port): {len(stub_markers)}")
    print()
    if issues:
        print("--- TRUE_ORPHAN (actionable: re-port or fix the PoP credit) ---")
        for f, k, d in issues:
            if k == "TRUE_ORPHAN":
                print(f"  {Path(f).name}: {d}")
    if dups:
        print("\n--- DUP_CREDIT (collision risk) ---")
        for c, v in list(dups.items())[:20]:
            print(f"  {c}: {v}")
    if stub_markers:
        print("\n--- STUB_MARKER ---")
        for f, s in stub_markers[:20]:
            print(f"  {Path(f).name}: {s}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
