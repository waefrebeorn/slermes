#!/usr/bin/env python3
"""refixture_gates.py — give a non-discriminating gate fixtures that move the answer.

Measured problem: a random sample of 40 PORT-class gates produced 26
NON-DISCRIMINATING and 5 TWIN BLIND verdicts and ZERO clean matches. The gates
link the port and are counted as evidence, but their fixtures are the generator's
placeholders ("hello", "world", "test123"), and both sides return the same constant
for every one of them. A gate whose answer never changes cannot prove parity.

This script attacks that mechanically: run the harness over a battery of inputs
chosen to hit different branches (empty, whitespace, digits, sign, JSON, path,
URL, credential shapes, unicode, very long), keep only the inputs whose cups
differ, and refuse the gate when the C side varies but the twinned Python side
does not (that is TWIN BLIND -- a fixture change cannot fix a blind twin).

It never invents an expectation: it only chooses inputs, then re-runs the gate so
the verdict is the runner's, not this script's.

Usage:
  python scripts/refixture_gates.py --from-provenance --limit 40 [--apply]
  python scripts/refixture_gates.py gate_a gate_b ... [--apply]
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
GATES = REPO / "tests" / "watercup" / "gates"
ROWS = REPO / "tests" / "watercup" / "build" / "gate_provenance.json"
sys.path.insert(0, str(REPO / "tests" / "watercup"))

BATTERY = [
    "", " ", "\t", "a", "A", "0", "1", "-1", "42", "3.14", "1e309", "NaN",
    "null", "None", "true", "false", "{}", "[]", '{"k": "v"}', "[1, 2]",
    "a b", "a,b", "a=b", "a:b", "a;b", "a|b", "a/b", "a\\b", "/etc/passwd",
    "C:\\Users\\x\\f.txt", "https://example.com/p?q=1", "user@example.com",
    "+1-555-0100", "-----BEGIN PRIVATE KEY-----", "sk-abcdef0123456789",
    "ghp_0123456789abcdefghij", "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxIn0.sig",
    "üñïçødé", "日本語", "%s", "%d", "${HOME}", "$HOME", "~/.ssh/id_rsa",
    "x" * 200, "a\nb", "  padded  ", "NULL", "-", "--flag=1", "key=secret",
    "Authorization: Bearer abc", "postgres://u:p@h:5432/db", "\r\n",
]





def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("gates", nargs="*")
    ap.add_argument("--from-provenance", action="store_true")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--min-distinct", type=int, default=2)
    ap.add_argument("--apply", action="store_true", help="write gate.json + re-run")
    args = ap.parse_args()

    import wc_run

    names = list(args.gates)
    if args.from_provenance:
        rows = json.loads(ROWS.read_text())
        names += [r["gate"] for r in rows if r["class"] == "PORT"]
    if args.limit:
        names = names[:args.limit]

    fixed = blind = kept = 0
    for name in names:
        gd = GATES / name
        g = json.loads((gd / "gate.json").read_text())
        if g.get("inputs_out_of_domain"):
            kept += 1
            print("kept  %-52s declares out-of-domain inputs" % name)
            continue
        try:
            out = wc_run.build_harness(gd, g, force=False)
        except Exception as e:
            kept += 1
            print("kept  %-52s build: %s" % (name, str(e)[:60]))
            continue
        # One input per process: the port may crash on hostile input, and a crash
        # is information (Python returned a value there) -- it must not wipe out
        # the whole battery.
        c_cups, crashes = [], []
        for probe in BATTERY:
            try:
                got = wc_run.run_harness(out, dict(g, inputs=[probe]))
                c_cups.append(got[0] if got else "EMPTY:%d" % len(c_cups))
            except Exception:
                crashes.append(probe)
                c_cups.append("CRASH:%d" % len(c_cups))
        # the twin's cups for the same battery, through the runner's own loader
        try:
            twin = wc_run.load_twin(g)
            inst = getattr(twin, "_t", twin)
            inst._cup_label = wc_run._harness_cup_label(gd, g) or g.get("c_fn")
            p_cups = list(wc_run._run_twin_inputs(twin, BATTERY))
        except Exception as e:
            kept += 1
            print("kept  %-52s twin: %s" % (name, str(e)[:60]))
            continue

        if crashes:
            print("note  %-52s port CRASHES on %d input(s): %r"
                  % (name, len(crashes), [c[:14] for c in crashes[:3]]))
        c_distinct = len(set(c_cups))
        p_distinct = len(set(p_cups))
        if c_distinct < args.min_distinct:
            kept += 1
            print("kept  %-52s C is constant over %d inputs (non-discriminating by "
                  "construction?)" % (name, len(BATTERY)))
            continue
        if p_distinct < args.min_distinct:
            blind += 1
            print("blind %-52s C varies, twin constant -- fixtures cannot fix this"
                  % name)
            continue

        # Keep the smallest set of inputs that still yields >= min-distinct cups on
        # BOTH sides: a greedy cover, so the gate stays readable.
        chosen: list[int] = []
        for i in range(len(BATTERY)):
            chosen.append(i)
            if len({c_cups[j] for j in chosen}) >= args.min_distinct and \
               len({p_cups[j] for j in chosen}) >= args.min_distinct:
                break
        # then add every remaining input that adds a NEW cup on either side
        seen_c = {c_cups[j] for j in chosen}
        seen_p = {p_cups[j] for j in chosen}
        for j in range(len(BATTERY)):
            if j in chosen:
                continue
            if c_cups[j] not in seen_c or p_cups[j] not in seen_p:
                chosen.append(j)
                seen_c.add(c_cups[j])
                seen_p.add(p_cups[j])
        new_inputs = [BATTERY[j] for j in sorted(chosen)]

        if not args.apply:
            print("plan  %-52s %d inputs, C %d distinct / twin %d distinct"
                  % (name, len(new_inputs), c_distinct, p_distinct))
            fixed += 1
            continue
        g["inputs"] = new_inputs
        (gd / "gate.json").write_text(json.dumps(g, indent=2) + "\n")
        fixed += 1

    print("\n%d refixtured, %d twin-blind, %d kept, of %d examined"
          % (fixed, blind, kept, len(names)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
