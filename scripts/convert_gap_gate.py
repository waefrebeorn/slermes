#!/usr/bin/env python3
"""convert_gap_gate.py — turn a GAP sentinel into a gate over the real port.

13,300 of the 22,603 water-cup gates are placeholders: their harness calls
`gap_sentinel()`, which returns the function's name for every input, because no C
port existed when the gate was generated. 8,451 of them name a (module, function)
that a `PoP:` annotation in src/ claims to implement, and 7,342 of those claims
point at a symbol the built port exports. None of that is evidence until a cup
comes out of the port and equals live Python on an input that changes the answer.

Conversion is per gate and evidence-gated:

  1. the symbol comes from the PoP annotation (`--symbol-map`), and only when it
     serves exactly one gate — `cmd_mcp` claimed by 16 gates is a dispatcher, not
     the function under test,
  2. the C signature is read from the annotation's source file: one `const char*`,
     or an out-buffer `(const char*, char*, size_t)`. Any other arity is reported,
     never guessed (the first sweep matched nothing for 23/25 gates because it
     assumed one argument everywhere),
  3. the live twin's own return value picks the rendering (bool -> True/False,
     int -> digits, str -> text) so both sides hash the same bytes,
  4. cups must equal the twin's AND discriminate. A function whose answer ignores
     its input matches trivially; two such conversions were accepted before this
     rule existed and were zero evidence.

Usage:
  python scripts/convert_gap_gate.py --symbol-map m.json --limit 40
A converted harness stays on disk; every failure is restored to its sentinel, so a
list can be rerun safely.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
GATES = REPO / "tests" / "watercup" / "gates"
ARCHIVE = REPO / "tests" / "watercup" / "build" / "_all_objects.a"
sys.path.insert(0, str(REPO / "tests" / "watercup"))

TEMPLATE = """/* gate: {name} — CONVERTED from a GAP sentinel to the port symbol {sym}.
 * PoP claim: {claim}
 * The sentinel returned the function name for every input; this calls {sym}().
 * Kept only because its cups equalled the LIVE Python twin's on inputs that
 * actually change the answer ({nvary} distinct cups over {n} inputs). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

{decl}

static void trace_one(const char *input) {{
    WCUP_BEGIN("{name}");
{body}
    char cup[64];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("{name}");
}}

int main(int argc, char **argv) {{
    if (argc < 3) {{ fprintf(stderr, "usage: %s <input>...\n", argv[0]); return 2; }}
    for (int i = 2; i < argc; i++) {{
        const char *input = strcmp(argv[i], "NULL") == 0 ? NULL : argv[i];
        trace_one(input);
    }}
    return 0;
}}
"""


def archive_symbols() -> set[str]:
    r = subprocess.run(["nm", "-g", "--defined-only", str(ARCHIVE)],
                       capture_output=True, text=True, errors="replace")
    out = set()
    for line in r.stdout.splitlines():
        p = line.split()
        if len(p) >= 3:
            out.add(p[2])
    return out


def c_signature(src_file: Path, sym: str):
    """(return type text, arity, has_out_buffer) read from the definition."""
    try:
        txt = src_file.read_text(errors="ignore")
    except OSError:
        return None
    m = re.search(r"^[^\n;{}]*\b%s\s*\(([^)]*)\)\s*\{" % re.escape(sym), txt, re.M)
    if not m:
        return None
    head = m.group(0)[:m.group(0).index(sym)]
    head = re.sub(r"\bstatic\b|\binline\b", " ", head).strip()
    params = m.group(1).strip()
    if params in ("", "void"):
        return head or "long", 0, False
    depth, n = 0, 1
    for ch in params:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        elif ch == "," and depth == 0:
            n += 1
    has_buf = bool(re.search(r"char\s*\*\s*\w+\s*,\s*(size_t|int|unsigned)", params))
    return head or "long", n, has_buf


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("gates", nargs="*")
    ap.add_argument("--from-list")
    ap.add_argument("--symbol-map", help="JSON {gate: {symbol, src}} from PoP annotations")
    ap.add_argument("--limit", type=int, default=0)
    args = ap.parse_args()

    names = list(args.gates)
    if args.from_list:
        names += [l.strip() for l in Path(args.from_list).read_text().splitlines() if l.strip()]
    if args.limit:
        names = names[:args.limit]

    import wc_run  # the exact build/link/twin path the fleet uses

    smap = {}
    if args.symbol_map:
        raw_map = json.loads(Path(args.symbol_map).read_text())
        counts = {}
        for v in raw_map.values():
            counts[v["symbol"]] = counts.get(v["symbol"], 0) + 1
        smap = {k: v for k, v in raw_map.items() if counts[v["symbol"]] == 1}

    converted, failed = [], []
    for name in names:
        gd = GATES / name
        g = json.loads((gd / "gate.json").read_text())
        original = (gd / "harness.c").read_text()
        if "gap_sentinel" not in original:
            failed.append((name, "harness is not a GAP sentinel"))
            continue
        if name not in smap:
            failed.append((name, "no unambiguous PoP symbol"))
            continue
        sym = smap[name]["symbol"]
        sig = c_signature(REPO / smap[name]["src"], sym)
        if sig is None:
            failed.append((name, "cannot read %s() in %s" % (sym, smap[name]["src"])))
            continue
        ret, arity, has_buf = sig
        if arity == 0:
            failed.append((name, "%s() takes no argument -- a string corpus cannot test it" % sym))
            continue
        if arity == 1 and not has_buf:
            shape = "single"
        elif has_buf and arity in (2, 3):
            shape = "buf"
        else:
            failed.append((name, "%s() takes %d args -- no arg builder" % (sym, arity)))
            continue

        try:
            twin = wc_run.load_twin(g)
            lbl = wc_run._harness_cup_label(gd, g) or g["c_fn"]
            inst = getattr(twin, "_t", twin)
            inst._cup_label = lbl
            kind = None
            for probe in g["inputs"]:
                try:
                    raw = twin.call(probe, g["inputs"], 0)
                except Exception:
                    continue
                if isinstance(raw, bool):
                    kind = "bool"
                elif isinstance(raw, int):
                    kind = "int"
                elif isinstance(raw, str):
                    kind = "str"
                if kind:
                    break
            if kind is None:
                failed.append((name, "the live twin returns no comparable value"))
                continue
            py_cups = wc_run._run_twin_inputs(twin, g["inputs"])
        except Exception as e:
            failed.append((name, "twin: %s: %s" % (type(e).__name__, e)))
            continue

        if shape == "single":
            decl = "extern %s %s(const char *input);" % (ret, sym)
            call = "%s(input)" % sym
            if kind == "str":
                body = "    const char *r = %s;\n    WCUP_VALSTR(r ? r : \"\");" % call
            elif kind == "bool":
                body = "    long r = (long)%s;\n    WCUP_VALSTR(r ? \"True\" : \"False\");" % call
            else:
                body = "    long r = (long)%s;\n    WCUP_VALINT(r);" % call
        else:
            decl = "extern %s %s(const char *input, char *buf, size_t cap);" % (ret, sym)
            body = ("    char buf[512];\n    memset(buf, 0, sizeof buf);\n"
                    "    %s(input, buf, sizeof buf);\n    WCUP_VALSTR(buf);" % sym)

        (gd / "harness.c").write_text(TEMPLATE.format(
            name=lbl, sym=sym, claim="src %s" % smap[name]["src"],
            nvary="?", n=len(g["inputs"]), decl=decl, body=body))
        ok, cups = False, []
        try:
            out = wc_run.build_harness(gd, g, force=True)
            cups = wc_run.run_harness(out, g)
            ok = cups == list(py_cups) and (len(g["inputs"]) < 2 or len(set(cups)) > 1)
        except Exception:
            ok = False
        if ok:
            (gd / "harness.c").write_text(TEMPLATE.format(
                name=lbl, sym=sym, claim="src %s" % smap[name]["src"],
                nvary=len(set(cups)), n=len(cups), decl=decl, body=body))
            converted.append((name, sym, shape, kind))
            print("CONVERTED %-58s %s [%s/%s]" % (name, sym, shape, kind))
        else:
            (gd / "harness.c").write_text(original)
            why = "compiled but cups did not match+discriminate"
            if cups and len(set(cups)) == 1:
                why = "constant answer -- the function ignores the input"
            failed.append((name, "%s(): %s" % (sym, why)))

    for name, why in failed:
        print("kept GAP  %-58s %s" % (name, why[:110]))
    print("\n%d converted, %d kept as gap, of %d tried"
          % (len(converted), len(failed), len(names)))
    print("(a converted harness stays on disk; every failure is restored to its sentinel)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
