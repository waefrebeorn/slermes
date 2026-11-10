#!/usr/bin/env python3
"""
backfill_oracle.py — generate an oracle for a harness that has none.

The biggest whole-project gap is not unimplemented stubs: it is the ~180 port
files that ALREADY implement C functions (with a harness t_port_<name>.c
emitting JSON) but were never given a differential oracle. This script closes
that gap in bulk: for a harness it finds the port header/object the harness
drives, reads the PoP `module.py:func` annotations from the port .c, ast-
extracts the LIVE Python defs, and writes tests/oracle_<name>.py with a correct
skeleton: a case loop over the harness's emitted lines plus a per-function
binding you finish (the arg-mapping from harness JSON fields to the Python
params is the only judgment the machine can't make).

It does NOT compile or gate — run_all_oracles.sh does that after you finish
each binding. Generated oracles are valid Python (py_compile clean) and carry a
clear `TODO(bind)` where the field->arg mapping must be completed.

Usage:
  python tests/backfill_oracle.py <name> [<name2> ...]     # generate drafts
  python tests/backfill_oracle.py --all                   # every unverified one
  python tests/backfill_oracle.py <name> --verify         # py_compile the result
"""
import argparse
import ast
import re
import sys
from pathlib import Path

SLERMES = Path("C:/Users/eman5/slermes")
HERMES = Path("C:/Users/eman5/hermes-agent")

PORT_C = SLERMES / "src" / "cli"
TESTS = SLERMES / "tests"
POP_RE = re.compile(r"/\*\s*PoP:\s*[\w]+\s*@\s*([\w/.]+\.py):([\w.]+)\s*\*/")
INCLUDE_RE = re.compile(r'#include\s*"([^"]+)"')


def existing_oracles():
    return {p.name[len("oracle_"):-3] for p in TESTS.glob("oracle_*.py")}


def harness_targets(hname: str):
    """Return list of (module,func) the harness drives, via its port .c's PoP."""
    hp = TESTS / f"t_port_{hname}.c"
    if not hp.is_file():
        return []
    htext = hp.read_text(encoding="utf-8", errors="replace")
    # only the port header the harness actually includes
    port_headers = [i for i in INCLUDE_RE.findall(htext) if i.startswith("port_")]
    found = []
    for ph in port_headers:
        # the header name "port_x.h" maps to the source "port_x.c" (strip .h)
        pc = PORT_C / f"{ph[:-2] if ph.endswith('.h') else ph}.c"
        if not pc.is_file():
            # maybe under a subdir
            base = ph[:-2] if ph.endswith(".h") else ph
            cands = list(SLERMES.glob(f"**/{base}.c"))
            pc = cands[0] if cands else None
        if not pc or not pc.is_file():
            continue
        ptext = pc.read_text(encoding="utf-8", errors="replace")
        found.extend((m.group(1), m.group(2)) for m in POP_RE.finditer(ptext))
    return found


def live_src(module: str, func: str):
    p = HERMES / module
    if not p.is_file():
        return None
    src = p.read_text(encoding="utf-8", errors="replace")
    try:
        tree = ast.parse(src)
    except SyntaxError:
        return None
    for n in ast.walk(tree):
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name == func:
            seg = ast.get_source_segment(src, n)
            if seg:
                return seg
    return None


def camel(s: str):
    parts = [p for p in re.split(r"_+", s) if p]
    return "".join(parts).lower() or "out"


def write_oracle(hname: str, targets) -> str:
    """Write tests/oracle_<hname>.py, return its path."""
    out_path = TESTS / f"oracle_{hname}.py"
    tp = "t_top" if False else "to   "
    lines = []
    lines.append("#!/usr/bin/env python3")
    lines.append(f'"""Oracle for {hname} (backfilled).')
    for mod, fn in targets:
        lines.append(f"  {mod}:{fn}")
    lines.append('"""')
    lines.append("import ast")
    lines.append("import json")
    lines.append("import sys")
    lines.append("")
    lines.append("")
    # ast-extract each target
    for mod, fn in targets:
        safe = camel(fn)
        lines.append(f'SRC_{safe.upper()} = "C:/Users/eman5/hermes-agent/{mod}"')
    lines.append("")
    binds = {}
    for mod, fn in targets:
        safe = camel(fn)
        binds[fn] = safe
        lines.append(f"def _load_{safe}():")
        lines.append(f'    src = open(SRC_{safe.upper()}, encoding="utf-8").read()')
        lines.append('    ns = {}  # TODO(seed): ns["Path"]=Path; ns["math"]=...; logger no-op; runtime fake')
        lines.append("    for n in ast.parse(src).body:")
        lines.append('        if isinstance(n, ast.FunctionDef) and n.name == "%s":' % fn)
        lines.append(f'            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC_{safe.upper()}, "exec"), ns)')
        lines.append(f'    return ns["{fn}"]')
        lines.append("")
    # main
    lines.append("def main() -> int:")
    lines.append("    FN = {")
    for fn, safe in binds.items():
        lines.append(f'        "{fn}": _load_{safe}(),')
    lines.append("    }")
    lines.append("")
    lines.append("    cases = []")
    lines.append("    for line in sys.stdin.read().splitlines():")
    lines.append("        line = line.strip()")
    lines.append("        if not line:")
    lines.append("            continue")
    lines.append("        try:")
    lines.append("            cases.append(json.loads(line))")
    lines.append("        except Exception:")
    lines.append("            pass")
    lines.append(f'    if not cases:')
    lines.append(f'        print("ORACLE {hname}: 0 cases (link failure in disguise)")')
    lines.append("        return 1")
    lines.append("    mism = 0")
    lines.append("    total = 0")
    lines.append("    for c in cases:")
    lines.append(f'        tag = c.get("tag") or c.get("case", "?")')
    lines.append("        total += 1")
    tag = camel(hname)
    lines.append('        # TODO(bind): map harness fields to the live python args below.')
    lines.append('        #   cur = FN["%s"](c["in"]...)  and compare to c["out"]' % (targets[0][1] if targets else "?"))
    lines.append('        want = FN[%r](**{k: c[k] for k in c if k not in ("tag", "case", "out")})' % (targets[0][1] if targets else "?"))
    lines.append("        want2 = want if isinstance(want, (str, bool, int, float)) or want is None else json.dumps(want)")
    lines.append("        if c.get(\"out\") != want2:")
    lines.append("            mism += 1")
    lines.append('            print(f"MISMATCH {c.get(chr(39)+\'case\'+chr(39))} C={c.get(chr(39)+\'out\'+chr(39))!r} PY={want2!r}")')
    lines.append(f"    print(f\"ORACLE {hname}: {{total}} cases, {{mism}} mismatches\")")
    lines.append("    return 1 if mism else 0")
    lines.append("")
    lines.append("")
    lines.append('if __name__ == "__main__":')
    lines.append("    sys.exit(main())")
    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    return str(out_path)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("names", nargs="*")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--verify", action="store_true")
    args = ap.parse_args()

    if args.all:
        existing = existing_oracles()
        names = sorted(
            p.name[len("t_port_"):-2]
            for p in TESTS.glob("t_port_*.c")
            if p.name[len("t_port_"):-2] not in existing
        )
    else:
        names = args.names
    if not names:
        print("no names and no --all; nothing to do")
        return 1

    ok = 0
    for nm in names:
        targets = harness_targets(nm)
        if not targets:
            print(f"SKIP {nm}: harness found but no PoP module:func in its port .c "
                  "(runtime-only port? add PoP to drive an oracle)")
            continue
        path = write_oracle(nm, targets)
        # py_compile verify
        if args.verify:
            import py_compile
            try:
                py_compile.compile(path, doraise=True)
                print(f"OK   {nm}: {path}  targets={targets}")
                ok += 1
            except py_compile.PyCompileError as e:
                print(f"ERR  {nm}: generated oracle failed to compile: {e}")
        else:
            print(f"OK   {nm}: {path}  targets={targets}")
            ok += 1
    print(f"\n{ok}/{len(names)} generated.")
    return 0


if __name__ == "__main__":
    sys.exit(main())