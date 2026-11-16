#!/usr/bin/env python3
"""Regenerate tests/c_standards_allowlist.py from the current tree.

The allowlist is DEBT, not policy: it records which files still violate
docs/dev/c-standards.md and why. Hand-editing it drifts; regenerating it from
the detector keeps the gate and the debt list in sync.

Run after fixing a batch of violations, then commit both files together:

    python scripts/gen_c_standards_allowlist.py
    python tests/verify_c_standards.py     # must PASS with 0 new

Never regenerate to make a NEW violation disappear -- the detector is the
standard's definition. If the detector is wrong, fix the detector.
"""
import collections
import importlib.util
import re
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
_spec = importlib.util.spec_from_file_location(
    "vcs", REPO / "tests/verify_c_standards.py")
m = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(m)

WHY = {
    "R1": "header does not name the unit (pre-standard)",
    "R2": "no PoP annotation (pre-standard)",
    "R3": "PoP C-slot ambiguous -- needs a human",
    "R4": "own header not included first (pre-standard)",
    "R5": "header guard off-standard (pre-standard)",
    "R6": "exported fn missing from its header",
    "R7": "formatting (pre-standard)",
    "R8": "PoP-annotated but not named port_<stem>.c (pre-standard)",
}


def main():
    findings = collections.defaultdict(list)
    for c in sorted(m.SRC.rglob("port_*.c")):
        rel = c.relative_to(m.REPO).as_posix()
        text = c.read_text(encoding="utf-8", errors="replace")
        stem = m.stem_of(c)
        if stem is None:
            continue

        head = text[:400]
        if not re.search(rf"\bport_{re.escape(stem)}\.c\b", head) or "/*" not in head[:60]:
            findings["R1"].append(rel)

        pops = m.POP.findall(text)
        if not pops:
            findings["R2"].append(rel)
        defined = set(m.DEF.findall(text)) - m.KEYWORDS
        for cn, pf, py in pops:
            if cn not in defined:
                findings["R3"].append(f"{rel}:{cn}")

        hdr = m.header_for(stem)
        if hdr is not None:
            hrel = hdr.relative_to(m.REPO).as_posix()
            incs = re.findall(r'^\s*#include\s+"([^"]+)"', text, re.M)
            if incs and incs[0] != hdr.name:
                findings["R4"].append(rel)
            htext = hdr.read_text(encoding="utf-8", errors="replace")
            if f"#ifndef PORT_{stem.upper()}_H" not in htext:
                findings["R5"].append(hrel)
            hnames = set(re.findall(r"\b([A-Za-z_]\w*)\s*\(", htext))
            for mm in m.DEF.finditer(text):
                fn = mm.group(1)
                if fn in m.KEYWORDS:
                    continue
                ls = text.rfind("\n", 0, mm.start(1)) + 1
                lp = text[ls:mm.start(1)]
                prev = text[:ls].rstrip().split("\n")[-1] if ls else ""
                if lp.lstrip().startswith("static ") or prev.lstrip().startswith("static "):
                    continue
                if fn not in hnames and fn != "main":
                    findings["R6"].append(f"{rel}:{fn}")

        b = m.check_r7(text)
        if b:
            findings["R7"].append(f"{rel}:{','.join(b[:3])}")

        pass  # R8 is computed in its own loop below

    for c in sorted(m.SRC.rglob("*.c")):
        if c.name.startswith("port_") or c.name.endswith("_ports.c"):
            continue
        if "PoP:" in c.read_text(encoding="utf-8", errors="replace"):
            findings["R8"].append(c.relative_to(m.REPO).as_posix())

    out = ['"""DEBT, not policy: files still violating docs/dev/c-standards.md.',
           "",
           "Regenerate with scripts/gen_c_standards_allowlist.py -- do not hand-edit.",
           "An entry whose file stops violating is reported as DEAD by the gate and",
           "fails the build, so debt must be paid to disappear.",
           '"""',
           "# rule -> {path: reason}",
           "ALLOW = {"]
    for r in ["R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8"]:
        out.append(f'    "{r}": {{')
        for v in sorted(set(findings.get(r, []))):
            out.append(f'        "{v}": "{WHY[r]}",')
        out.append("    },")
    out.append("}")
    dest = REPO / "tests/c_standards_allowlist.py"
    dest.write_bytes(("\n".join(out) + "\n").encode("utf-8"))
    total = sum(len(set(v)) for v in findings.values())
    print(f"allowlist: {total} entries -> {dest.relative_to(REPO).as_posix()}")
    for r in sorted(findings):
        print(f"  {r}: {len(set(findings[r]))}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
