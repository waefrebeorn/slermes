#!/usr/bin/env python3
"""
close_partials.py — close PARTIAL parity gaps by inserting the missing
`/* PoP: c_func @ module.py:py_fn */` annotation immediately before the
C function definition.

Driven by the live scanner JSON (tests/slermes_parity_battleground.py --json).
For every gap classified PARTIAL, the scanner already knows the C function
name (c_function), its primary location (c_location), the Python module
(module) and the Python feature (py_fn). The fix is pure annotation: add a
self-contained PoP line right before the function definition.

Safety rules (from slermes doc discipline):
- Never insert INTO an existing `/* ... */` block (would corrupt it).
- If the function already has an adjacent PoP for the same (c_fn, py_fn),
  skip (idempotent).
- If the function already has an adjacent PoP for the same c_fn but a
  DIFFERENT py_fn, APPEND a second PoP line (one C fn may port many py fns).
- Only annotate the actual definition (signature line ending in `(` with the
  function name as a word boundary), not a call site.

Usage:
    python3 scripts/close_partials.py [--apply] [--json <path>]
Without --apply: prints the edits it WOULD make (dry run).
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

POP_RE = re.compile(r'/\*\s*PoP:\s*(?P<cf>\w+)\s*@\s*(?P<mod>[\w./-]+?):(?P<py>\w+)\s*\*/')

def load_partials(json_path):
    d = json.load(open(json_path))
    out = []
    mods = d.get("modules", {})
    for module, m in mods.items():
        for g in m.get("gaps", []):
            if g.get("classification") != "PARTIAL":
                continue
            py = (g.get("python_feature") or {})
            pop = g.get("pop_annotation") or {}
            cf = g.get("c_function") or pop.get("c_function")
            loc = g.get("c_location") or pop.get("c_file")
            if not cf:
                continue
            out.append({
                "module": module,
                "py_fn": py.get("name"),
                "c_function": cf,
                "c_location": loc,
            })
    return out

def find_def_line(lines, c_fn):
    """Return index of the function definition line for c_fn, or None."""
    # Match a definition: optional qualifiers, return type, the name as a
    # whole word, then '(' somewhere on that line. Exclude obvious call sites
    # by requiring the name be preceded by whitespace/type and the '(' to be
    # on the same line.
    pat = re.compile(r'^\s*(?:static\s+|inline\s+|extern\s+|LIB_[\w]+\s*)*'
                     r'[A-Za-z_][\w\s\*]*\b' + re.escape(c_fn) + r'\s*\(')
    best = None
    for i, ln in enumerate(lines):
        if pat.match(ln):
            # crude guard: not a comment line
            stripped = ln.lstrip()
            if stripped.startswith('/*') or stripped.startswith('*') or stripped.startswith('//'):
                continue
            best = i
    return best

def already_annotated(adjacent_lines, c_fn, module, py_fn):
    for ln in adjacent_lines:
        m = POP_RE.search(ln)
        if m and m.group("cf") == c_fn and m.group("mod") == module and m.group("py") == py_fn:
            return True
    return False

def adjacent_pops(lines, def_idx):
    """Return list of indices of consecutive PoP lines immediately above def."""
    idxs = []
    i = def_idx - 1
    while i >= 0:
        s = lines[i].strip()
        if s == "":
            i -= 1
            continue
        if POP_RE.search(lines[i]):
            idxs.append(i)
            i -= 1
            continue
        break
    return list(reversed(idxs))

def process(items, apply):
    edits = 0
    skipped = 0
    by_file = {}
    for it in items:
        cf, loc, module, py = it["c_function"], it["c_location"], it["module"], it["py_fn"]
        if not loc or not os.path.isfile(os.path.join(ROOT, loc)):
            # fall back: grep whole src tree
            found = None
            for dirpath, _, fnames in os.walk(os.path.join(ROOT, "src")):
                for fn in fnames:
                    if fn.endswith(".c") or fn.endswith(".h"):
                        p = os.path.join(dirpath, fn)
                        try:
                            lst = open(p, encoding="utf-8", errors="ignore").read().splitlines()
                        except Exception:
                            continue
                        di = find_def_line(lst, cf)
                        if di is not None:
                            found = (os.path.relpath(p, ROOT), lst, di)
                            break
                if found:
                    break
            if not found:
                print(f"  SKIP (no def found): {cf} @ {module}:{py}")
                skipped += 1
                continue
            loc, lst, di = found
        else:
            p = os.path.join(ROOT, loc)
            lst = open(p, encoding="utf-8", errors="ignore").read().splitlines()
            di = find_def_line(lst, cf)
            if di is None:
                print(f"  SKIP (def not in {loc}): {cf} @ {module}:{py}")
                skipped += 1
                continue

        # Build the PoP line
        pop_line = f"/* PoP: {cf} @ {module}:{py} */"

        # Inspect adjacent PoP lines above the def
        adj = adjacent_pops(lst, di)
        adj_lines = [lst[i] for i in adj]
        if already_annotated(adj_lines, cf, module, py):
            skipped += 1
            continue

        by_file.setdefault(loc, []).append((di, pop_line, module, py))
        edits += 1

    # Apply / report per file
    for loc, edl in sorted(by_file.items()):
        edl.sort(reverse=True)  # apply from bottom to keep indices valid
        p = os.path.join(ROOT, loc)
        lst = open(p, encoding="utf-8", errors="ignore").read().splitlines()
        for di, pop_line, module, py in edl:
            if apply:
                lst.insert(di, pop_line)
        if apply:
            with open(p, "w", encoding="utf-8") as f:
                f.write("\n".join(lst) + "\n")
        print(f"{'APPLIED' if apply else 'WOULD '} {loc}: +{len(edl)} PoP"
              f"  ({', '.join(f'{m}:{x}' for _,_,m,x in edl)})")
    print(f"\nSummary: {edits} edits, {skipped} skipped (already done / not found).")
    return edits

if __name__ == "__main__":
    apply = "--apply" in sys.argv
    jp = "--json" in sys.argv and sys.argv[sys.argv.index("--json")+1] or "/tmp/parity_live.json"
    items = load_partials(jp)
    print(f"Loaded {len(items)} PARTIAL gaps from {jp}")
    process(items, apply)
