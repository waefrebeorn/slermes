#!/usr/bin/env python3
"""
close_gap.py — gap-closing toolkit for the slermes C port.

WHY: the parity battleground (tests/slermes_parity_battleground.py --json)
emits thousands of REAL_GAP features, but they are not equally actionable from
THIS C-only checkout. The upstream Python sources are not vendored here, so a
REAL_GAP is only *directly closeable* when:

  (a) the module already has a C port file under src/ (so the missing function
      can be added to a real, compiling translation unit), and
  (b) ideally there is an oracle fixture dir under tests/oracle/fixtures/<name>
      so the closure can be verified, not just asserted.

This tool turns the raw scanner JSON into an actionable worklist and scaffolds
the mechanical part of a closure (the PoP-annotated C function header + a
clearly-marked IMPLEMENT body + an oracle harness stub when fixtures exist).
It never fabricates behavior: the generated bodies say `// IMPLEMENT:` and the
implementer fills them from the upstream Python source. That is the angel move
— do the mechanical, index-fragmentation-prone part correctly so no one
double-codes, and leave only the behavior (which needs the Python) explicit.

Subcommands
-----------
  report            Print the closeable-gap worklist (default).
  scaffold <module> Generate scripts/generated/<module>_scaffold.c with PoP-
                    annotated stubs for every REAL_GAP in <module>, plus an
                    oracle harness stub if fixtures exist.
  verify <module>   Re-run the oracle for <module> (if harness + fixtures both
                    exist) and print MATCH/MISSING, so a closure is checkable.

The scanner is the single source of truth; nothing is hand-transcribed.
"""
import json
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCANNER = os.path.join(REPO, "tests", "slermes_parity_battleground.py")
SRC = os.path.join(REPO, "src")
FIX = os.path.join(REPO, "tests", "oracle", "fixtures")
GEN = os.path.join(REPO, "scripts", "generated")


def load():
    out = subprocess.check_output([sys.executable, SCANNER, "--json"], cwd=REPO)
    return json.loads(out, strict=False)["modules"]


def c_port_for(module_key):
    """Map a parity module key (e.g. 'tools/read_extract.py') to a C port file
    under src/ if one exists. Heuristic: strip the .py, replace '/' with '_',
    prefix 'port_' if needed; also try the known port_tools_* naming."""
    base = module_key.replace(".py", "").replace("/", "_")
    candidates = [
        os.path.join(SRC, base + ".c"),
        os.path.join(SRC, "tools", base + ".c"),
        os.path.join(SRC, "tools", "port_" + base + ".c"),
        os.path.join(SRC, "cli", base + ".c"),
        os.path.join(SRC, "agent", base + ".c"),
        os.path.join(SRC, "gateway", base + ".c"),
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    # broader search: any src .c whose basename contains the module basename
    modbase = os.path.basename(base)
    for root, _, files in os.walk(SRC):
        for f in files:
            if f.endswith(".c") and modbase in f:
                return os.path.join(root, f)
    return None


def fixture_for(module_key):
    name = os.path.basename(module_key.replace(".py", ""))
    d = os.path.join(FIX, name)
    return d if os.path.isdir(d) else None


def harness_for(module_key):
    name = os.path.basename(module_key.replace(".py", ""))
    h = os.path.join(REPO, "tests", "t_port_%s.c" % name)
    return h if os.path.exists(h) else None


def real_gaps(mods):
    rows = []
    for k, v in mods.items():
        gaps = [g for g in v.get("gaps", []) if g.get("classification") == "REAL_GAP"]
        if not gaps:
            continue
        cport = c_port_for(k)
        fix = fixture_for(k)
        harn = harness_for(k)
        pure = sum(1 for g in gaps if is_pure_gap(g))
        rows.append(
            {
                "module": k,
                "rg": len(gaps),
                "pure": pure,
                "cport": cport,
                "fixture": fix,
                "harness": harn,
                "gaps": gaps,
            }
        )
    # closeable-first: has C port, then has oracle (verifiable), then by size
    rows.sort(key=lambda r: (r["cport"] is None, r["harness"] is None, -r["rg"]))
    return rows


def is_pure_gap(gap):
    """A gap is 'pure' if it's a plain non-async helper with no class/decorators.

    Pure gaps are the mechanical closes: small functions with no IO/network
    deps that can be ported and oracle-verified in minutes.
    """
    feat = gap.get("python_feature", {})
    if feat.get("is_async") or feat.get("parent_class") or feat.get("decorators"):
        return False
    return True


def cmd_report(mods):
    rows = real_gaps(mods)
    closeable = [r for r in rows if r["cport"]]
    verifiable = [r for r in closeable if r["harness"]]
    total_pure = sum(r["pure"] for r in rows)
    print("=== close_gap report ===")
    print("modules with REAL_GAP : %d" % len(rows))
    print("  pure-leaf gaps (mechanical closes) : %d" % total_pure)
    print("  closeable here (C port exists) : %d" % len(closeable))
    print("    verifiable (oracle harness present) : %d" % len(verifiable))
    print()
    print("%-42s rg  pure  cport?  oracle?" % "module")
    print("-" * 72)
    for r in rows:
        c = "Y" if r["cport"] else "-"
        o = "Y" if r["harness"] else "-"
        print("%-42s %2d  %4d    %s      %s" % (r["module"][:42], r["rg"], r["pure"], c, o))
    print()
    print("Top closeable+verifiable targets:")
    for r in verifiable[:12]:
        print("  %s  (%d gaps, %d pure)" % (r["module"], r["rg"], r["pure"]))
    return 0


def cmd_scaffold(mods, module):
    rows = {r["module"]: r for r in real_gaps(mods)}
    if module not in rows:
        # allow match by suffix
        hits = [k for k in rows if module in k]
        if len(hits) == 1:
            module = hits[0]
        elif hits:
            print("ambiguous: %s" % hits)
            return 2
        else:
            print("no REAL_GAP module matching %r" % module)
            return 2
    r = rows[module]
    if not r["cport"]:
        print("module %s has no C port in src/ — cannot scaffold here" % module)
        print("(the upstream Python source is not vendored in this checkout)")
        return 1
    os.makedirs(GEN, exist_ok=True)
    out_c = os.path.join(GEN, os.path.basename(r["cport"]).replace(".c", "_scaffold.c"))
    lines = [
        "/* AUTO-SCAFFOLD from close_gap.py — do NOT hand-edit the IMPLEMENT bodies.",
        " * Source module: %s" % module,
        " * C port file : %s" % os.path.relpath(r["cport"], REPO),
        " * These stubs carry correct PoP annotations so the parity scanner can",
        " * later classify them PORTED once the IMPLEMENT body is filled from the",
        " * upstream Python source. They compile to safe defaults; they are NOT",
        " * behavior-complete.",
        " */",
        "",
    ]
    for g in r["gaps"]:
        pf = g["python_feature"]
        name = pf.get("name")
        kind = pf.get("kind", "function")
        lineno = pf.get("line_number")
        if kind == "function":
            lines.append("/* Port of Python: %s (tools-style helper, line %s) */" % (name, lineno))
            lines.append("/* TODO(close_gap): implement %s from upstream Python source */" % name)
            lines.append("void %s(void) {" % name)
            lines.append("    /* IMPLEMENT: port Python behavior here */")
            lines.append("}")
            lines.append("")
        elif kind == "class":
            lines.append("/* Port of Python class: %s (line %s) */" % (name, lineno))
            lines.append("/* TODO(close_gap): implement class %s */" % name)
            lines.append("")
        else:
            lines.append("/* Port of Python %s: %s (line %s) */" % (kind, name, lineno))
            lines.append("")
    open(out_c, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print("wrote %s" % out_c)
    print("  %d REAL_GAP(s) scaffolded for %s" % (len(r["gaps"]), module))
    if r["fixture"] and not r["harness"]:
        print("  NOTE: fixtures exist at %s but no tests/t_port_*.c harness —" % os.path.relpath(r["fixture"], REPO))
        print("        an oracle harness can be scaffolded to make this verifiable.")
    return 0


def cmd_verify(mods, module):
    rows = {r["module"]: r for r in real_gaps(mods)}
    hits = [k for k in rows if module in k]
    if not hits:
        print("no module matching %r" % module)
        return 2
    for k in hits:
        r = rows[k]
        if r["harness"] and r["fixture"]:
            rc = subprocess.call(
                ["bash", "tests/oracle/runners/run_oracle.sh",
                 os.path.basename(k.replace(".py", ""))],
                cwd=REPO,
            )
            print("%s -> oracle rc=%d" % (k, rc))
        else:
            print("%s -> no runnable oracle (harness=%s fixture=%s)"
                  % (k, "Y" if r["harness"] else "N", "Y" if r["fixture"] else "N"))
    return 0


def main():
    mods = load()
    if len(sys.argv) < 2 or sys.argv[1] == "report":
        return cmd_report(mods)
    sub = sys.argv[1]
    if sub == "scaffold":
        if len(sys.argv) < 3:
            print("usage: close_gap.py scaffold <module>")
            return 2
        return cmd_scaffold(mods, sys.argv[2])
    if sub == "verify":
        if len(sys.argv) < 3:
            print("usage: close_gap.py verify <module>")
            return 2
        return cmd_verify(mods, sys.argv[2])
    print("unknown subcommand %r" % sub)
    return 2


if __name__ == "__main__":
    sys.exit(main())
