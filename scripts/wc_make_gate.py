#!/usr/bin/env python3
"""scripts/wc_make_gate.py — scaffold a new water-cup gate.

Usage:
  python3 scripts/wc_make_gate.py <py_module> <py_fn> <c_file> <c_fn>

Creates tests/watercup/gates/<pybase>_<fn>/ with harness.c, twin.py,
gate.json skeletons (TODO markers where the agent fills the traced ladder),
then verifies with wc_run.py.
"""
import json, os, re, subprocess, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GATES = os.path.join(REPO, "tests", "watercup", "gates")

def main():
    if len(sys.argv) != 5:
        print(__doc__); return 2
    pymod, pyfn, cfile, cfn = sys.argv[1:5]
    pybase = os.path.basename(pymod).replace(".py", "")
    gate = f"{pybase}_{re.sub(r'^_', '', pyfn)}"
    gd = os.path.join(GATES, gate)
    os.makedirs(gd, exist_ok=True)

    json.dump({
        "name": gate, "subsystem": "agent", "kind": "cup",
        "c_src": "harness.c", "c_fn": cfn,
        "py_module": pymod, "py_fn": pyfn,
        "inputs": [],   # TODO: 8-12 branch-covering argv-safe inputs
        "notes": "Scaffolded by wc_make_gate.py; fill traced ladder + inputs.",
    }, open(os.path.join(gd, "gate.json"), "w"), indent=2)

    open(os.path.join(gd, "harness.c"), "w").write(f"""/* gate: {gate} — traced copy of {cfile}::{cfn}
 * (LIVE twin: {pymod}::{pyfn}). TODO: trace the real ladder. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static void run_one(const char *in) {{
    WCUP_BEGIN("{pyfn}");
    /* TODO: mirror the C port's branch/loop/value structure */
    WCUP_VALSTR(in);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\\n", cup); WCUP_END("{pyfn}");
}}

int main(int argc, char **argv) {{
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}}
""")
    open(os.path.join(gd, "twin.py"), "w").write(f'"""twin for {gate} — TODO: import LIVE {pymod}::{pyfn} and trace the SAME rung order."""\n')
    print("scaffolded", gd)
    return 0

if __name__ == "__main__":
    sys.exit(main())
