#!/usr/bin/env python3
"""convert_gap_gate.py — turn a GAP-sentinel gate into a gate over the real port.

11,400 of the 22,603 water-cup gates are placeholders: their harness calls
`gap_sentinel()`, which returns the function's name for every input, because no C
port existed when the gate was generated. Every one of those is a green-looking
line that measures nothing.

This converts one gate at a time, and only when the evidence says it can:

  1. the gate's c_fn (or its module-prefixed alias) exists in the port archive,
  2. a call shape actually compiles and links (const char* -> long / bool /
     const char* / out-buffer),
  3. the resulting cups equal the LIVE Python twin's cups for the gate's inputs.

Only then is the harness written. Anything that fails keeps its sentinel and is
reported with the reason, so a conversion is never a guess.

Usage:
  python scripts/convert_gap_gate.py <gate> [<gate> ...] [--write]
  python scripts/convert_gap_gate.py --from-list ready.txt [--write]
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
GATES = REPO / "tests" / "watercup" / "gates"
ARCHIVE = REPO / "tests" / "watercup" / "build" / "_all_objects.a"
sys.path.insert(0, str(REPO / "tests" / "watercup"))

VARIANTS = {
    # bool-ish: Python renders True/False, so the C side must too.
    "bool": '''extern int {sym}(const char *input);
    int r = {sym}(input);
    WCUP_VALSTR(r ? "True" : "False");''',
    # integer: str(5) == "5" on both sides.
    "int": '''extern long {sym}(const char *input);
    long r = {sym}(input);
    WCUP_VALINT(r);''',
    # string return
    "str": '''extern const char *{sym}(const char *input);
    const char *r = {sym}(input);
    WCUP_VALSTR(r ? r : "");''',
}

TEMPLATE = '''/* gate: {name} — CONVERTED from a GAP sentinel to the real port symbol {sym}.
 * The sentinel returned the function name for every input; this calls {sym}().
 * Kept only because its cups equalled the LIVE Python twin's with this shape. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

static void trace_one(const char *input) {{
    WCUP_BEGIN("{name}");
{body}
    char cup[64];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\\n", cup);
    WCUP_END("{name}");
}}

int main(int argc, char **argv) {{
    if (argc < 3) {{ fprintf(stderr, "usage: %s <input>...\\n", argv[0]); return 2; }}
    for (int i = 2; i < argc; i++) {{
        const char *input = strcmp(argv[i], "NULL") == 0 ? NULL : argv[i];
        trace_one(input);
    }}
    return 0;
}}
'''


def archive_symbols() -> set[str]:
    r = subprocess.run(["nm", "-g", "--defined-only", str(ARCHIVE)],
                       capture_output=True, text=True, errors="replace")
    out = set()
    for line in r.stdout.splitlines():
        p = line.split()
        if len(p) >= 3:
            out.add(p[2])
    return out


def candidate_symbols(cfn: str, syms: set[str]) -> list[str]:
    """Exact name first, then unambiguous module-prefixed aliases."""
    base = cfn.split(".")[-1]
    if not base or base.startswith("__"):
        return []
    if base in syms:
        return [base]
    aliases = sorted(s for s in syms if s.endswith("_" + base) or s.endswith("__" + base))
    return aliases if len(aliases) == 1 else []


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("gates", nargs="*")
    ap.add_argument("--from-list")
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--limit", type=int, default=0)
    args = ap.parse_args()

    names = list(args.gates)
    if args.from_list:
        names += [l.strip() for l in Path(args.from_list).read_text().splitlines() if l.strip()]
    if args.limit:
        names = names[:args.limit]

    import wc_run  # reuse the exact build/link/twin path the fleet uses

    syms = archive_symbols()
    converted, failed = [], []
    for name in names:
        gd = GATES / name
        g = json.loads((gd / "gate.json").read_text())
        cands = candidate_symbols(g.get("c_fn") or "", syms)
        if not cands:
            failed.append((name, "no port symbol for c_fn=%r" % g.get("c_fn")))
            continue
        sym = cands[0]
        original = (gd / "harness.c").read_text()
        if "gap_sentinel" not in original:
            failed.append((name, "harness is not a GAP sentinel"))
            continue
        # Twin cups for the same inputs: the acceptance oracle.
        try:
            twin = wc_run.load_twin(g)
            _lbl = wc_run._harness_cup_label(gd, g) or g["c_fn"]
            inst = getattr(twin, "_t", twin)
            inst._cup_label = _lbl
            py_cups = wc_run._run_twin_inputs(twin, g["inputs"])
        except Exception as e:
            failed.append((name, "twin: %s: %s" % (type(e).__name__, e)))
            continue

        ok = None
        for kind, body in VARIANTS.items():
            (gd / "harness.c").write_text(
                TEMPLATE.format(name=_lbl, sym=sym,
                                body="\n".join(body.format(sym=sym).splitlines())))
            try:
                out = wc_run.build_harness(gd, g, force=True)
                cups = wc_run.run_harness(out, g)
            except Exception as e:
                continue
            if cups != list(py_cups):
                continue
            # Matching cups are NOT enough: a function whose answer does not depend
            # on the input matches trivially, and two such conversions (a config
            # getter and a model-id lookup) were accepted here before this rule
            # existed. They printed one cup for five inputs -- zero evidence.
            if len(g["inputs"]) >= 2 and len(set(cups)) == 1:
                continue
            ok = kind
            break
        if ok:
            converted.append((name, sym, ok))
            print("CONVERTED %-55s %s [%s]" % (name, sym, ok))
        else:
            (gd / "harness.c").write_text(original)  # restore the sentinel
            failed.append((name, "no discriminating call shape matched the twin"))

    for name, why in failed:
        print("kept GAP  %-55s %s" % (name, why))
    print("\n%d converted, %d kept as gap, of %d tried"
          % (len(converted), len(failed), len(names)))
    print("(a converted harness is already on disk; every failure was restored to its "
          "sentinel, so a list can be rerun safely)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
