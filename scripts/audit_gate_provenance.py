#!/usr/bin/env python3
"""audit_gate_provenance.py — where does a gate's C answer actually come from?

A cup gate prints two numbers: one from a C program, one from live Python. The
Python side is the oracle. The C side is only EVIDENCE ABOUT THE PORT when it came
out of the port. Three shapes exist in this tree, and they look identical in a
green report:

  PORT   harness.c references a symbol defined in src/ (or lib/) and does not
         define it itself          -> the cups measure the shipped port
  COPY   harness.c defines the traced function locally ("traced copy of
         src/...")                 -> the cups measure a hand transcription made
                                      by the same agent that wrote the test, so a
                                      green line proves the TRANSCRIPTION agrees
                                      with Python, not the port
  SHIM   a gate-local object supplies the symbol the port also defines
         (see audit_gate_shims.py) -> the cups measure the gate

Only PORT gates are parity evidence. The COPY class is large; the point of this
script is to make it countable and to keep it from growing silently.

Usage:
  python scripts/audit_gate_provenance.py                # summary
  python scripts/audit_gate_provenance.py --list COPY    # names, one per line
  python scripts/audit_gate_provenance.py --json out.json
"""
from __future__ import annotations

import argparse
import os
import json
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
GATES = REPO / "tests" / "watercup" / "gates"
ARCHIVE = REPO / "tests" / "watercup" / "build" / "_all_objects.a"

DEF_RE = re.compile(r"^[A-Za-z_][\w\s\*]*?\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{", re.M)
C_KEYWORDS = {
    "if", "for", "while", "switch", "return", "sizeof", "do", "else", "main",
    "printf", "fprintf", "snprintf", "sprintf", "memset", "memcpy", "strlen",
    "strcmp", "strncmp", "strcpy", "strncpy", "strtok_r", "atoi", "atof",
    "strtod", "malloc", "calloc", "realloc", "free", "tmpfile",
}


def port_symbols() -> set[str]:
    # Rebuilds the archive when stale: a stale archive reported fresh symbols absent
    # (backup_should_exclude is T in backup.o and was reported missing).
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from _gate_audit_common import port_symbols as _ps
    return _ps()


QUARRY = Path(os.environ.get("HERMES_QUARRY", r"C:\Users\eman5\hermes-agent"))
_EXISTS: dict[str, bool] = {}


def _oracle_exists(rel: str) -> bool:
    """Is the Python file this gate names still present in the quarry?

    777 GAP gates name paths like `plugins/platforms/signal/adapter.py` that no
    longer exist. Their oracle is gone, so no fixture work can ever close them --
    they are STALE, not gaps, and lumping them in with the 13,937 inflates the
    work that is actually doable.
    """
    if not rel or not rel.endswith(".py"):
        return True
    if rel not in _EXISTS:
        _EXISTS[rel] = (QUARRY / rel).is_file()
    return _EXISTS[rel]



_AST_CACHE: dict[str, object] = {}


def _fn_ignores_input(py_module: str, py_fn: str) -> bool | None:
    """True when the Python function never reads its own parameter.

    Such a gate cannot discriminate however good its fixtures are: the answer does
    not depend on the input. Measured motivation: a random 40-gate PORT sample gave
    26 NON-DISCRIMINATING, and refixturing 26 of them with a 56-input battery fixed
    exactly 1 -- 21 were constant functions, not bad fixtures. Static here, so the
    fleet can be qualified without running 8,000 harnesses.
    """
    import ast
    rel = (py_module or "").strip()
    if not rel.endswith(".py"):
        return None
    if rel not in _AST_CACHE:
        try:
            _AST_CACHE[rel] = ast.parse((QUARRY / rel).read_text(encoding="utf-8",
                                                                errors="ignore"))
        except (OSError, SyntaxError):
            _AST_CACHE[rel] = None
    tree = _AST_CACHE[rel]
    if tree is None:
        return None
    name = (py_fn or "").strip().split(".")[-1]
    node = None
    for n in tree.body:
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name == name:
            node = n
            break
    if node is None:
        return None
    args = [a.arg for a in node.args.posonlyargs + node.args.args + node.args.kwonlyargs]
    args = [a for a in args if a not in ("self", "cls")]
    if not args:
        return True
    used = {n.id for n in ast.walk(node) if isinstance(n, ast.Name)}
    return not any(a in used for a in args)


def _returns_tuple(py_module: str, py_fn: str) -> bool | None:
    """Does the Python fn return a tuple (i.e. does the shape differ from a scalar)?

    The generated twin mixes str(tuple) while the generated harness mixes the C
    return value, so such a gate hashes two different shapes and can never match --
    it reads as a divergence no matter what the port does (see
    hermes_cli_codex_runtime_switch_py__check_codex_binary_ok).
    """
    import ast
    rel = (py_module or "").strip()
    if not rel.endswith(".py") or rel not in _AST_CACHE:
        if rel.endswith(".py") and rel not in _AST_CACHE:
            try:
                _AST_CACHE[rel] = ast.parse((QUARRY / rel).read_text(encoding="utf-8",
                                                                    errors="ignore"))
            except (OSError, SyntaxError):
                _AST_CACHE[rel] = None
        if rel not in _AST_CACHE:
            return None
    tree = _AST_CACHE.get(rel)
    if tree is None:
        return None
    name = (py_fn or "").strip().split(".")[-1]
    for n in tree.body:
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name == name:
            vals = [r.value for r in ast.walk(n) if isinstance(r, ast.Return) and r.value]
            if not vals:
                return None
            return all(isinstance(v, ast.Tuple) or
                       (isinstance(v, ast.Call) and getattr(v.func, "id", "") == "tuple")
                       for v in vals)
    return None


def classify(gdir: Path, port: set[str], shim_names: set[str]) -> tuple[str, str]:
    g = json.loads((gdir / "gate.json").read_text())
    cfn = g.get("c_fn") or ""
    try:
        src = (gdir / "harness.c").read_text(errors="ignore")
    except OSError:
        return "BROKEN", "no harness.c"
    txt = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    txt = re.sub(r"//[^\n]*", " ", txt)
    defined = {m.group(1) for m in DEF_RE.finditer(txt)} - C_KEYWORDS
    base = cfn.split(".")[-1]
    # An explicit, auditable exclusion: the gate cannot discriminate or cannot be
    # ported at all (no input to vary, or a Python-only probe). Without this the
    # fleet counts it as a PORT and the number lies upward.
    if (g.get("na_reason") or "").strip():
        return "NA", base
    if "gap_sentinel" in txt:
        if not _oracle_exists((g.get("py_module") or "").strip()):
            return "STALE", base
        return "GAP", base
    if base and base in shim_names:
        return "SHIM", base
    if base and base in defined:
        # The harness ships its own implementation: the cups measure the
        # transcription, whatever the port does.
        return "COPY", base
    if base and base in port:
        return "PORT", base
    if base and re.search(r"\b%s\s*\(" % re.escape(base), txt):
        return "MISSING", base  # calls a symbol the built port does not export
    return "UNKNOWN", base


def _input_used(gdir: Path) -> bool:
    """Does the harness body actually use its per-case argument?

    gateway_acp_acp_get_id calls acp_get_id(NULL) for every input, so no fixture
    can ever make it discriminate -- it is non-discriminating BY CONSTRUCTION, not
    by bad fixtures. Cheap tell: the parameter name appears only in its own
    declaration inside the traced function.
    """
    try:
        src = (gdir / "harness.c").read_text(errors="ignore")
    except OSError:
        return True
    m = re.search(r"static\s+void\s+trace_one\s*\(\s*(?:const\s+)?char\s*\*\s*(\w+)\s*\)",
                  src)
    if not m:
        return True
    name = m.group(1)
    start = src.index("{", m.end())
    depth, i = 0, start
    while i < len(src):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                break
        i += 1
    body = src[start:i]
    return len(re.findall(r"\b%s\b" % re.escape(name), body)) > 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", metavar="CLASS", help="print gate names of one class")
    ap.add_argument("--json", metavar="PATH", help="write per-gate rows")
    args = ap.parse_args()

    port = port_symbols()
    rows = []
    for gdir in sorted(GATES.iterdir()):
        if not (gdir / "gate.json").is_file():
            continue
        shim_names = set()
        shim = gdir / "shims.c"
        if shim.exists():
            txt = shim.read_text(errors="ignore")
            shim_names = {m.group(1) for m in DEF_RE.finditer(txt)} - C_KEYWORDS
        cls, sym = classify(gdir, port, shim_names)
        gj = json.loads((gdir / "gate.json").read_text())
        rows.append({"gate": gdir.name, "class": cls, "symbol": sym,
                     "input_used": _input_used(gdir),
                     "input_independent": (_fn_ignores_input(gj.get("py_module") or "",
                                                             gj.get("py_fn") or "")
                                          if cls == "PORT" else None),
                     "returns_tuple": (_returns_tuple(gj.get("py_module") or "",
                                                      gj.get("py_fn") or "")
                                       if cls == "PORT" else None)})

    counts = Counter(r["class"] for r in rows)
    if args.list:
        for r in rows:
            if r["class"] == args.list:
                print(r["gate"])
        return 0
    if args.json:
        Path(args.json).write_text(json.dumps(rows, indent=1))
        print("wrote", args.json)
    total = len(rows)
    for cls, n in counts.most_common():
        print("%-8s %6d  %5.1f%%" % (cls, n, 100.0 * n / max(total, 1)))
    print("total    %6d" % total)
    blind = sum(1 for r in rows if not r["input_used"])
    print("\ninput ignored by the harness: %d gate(s) -- non-discriminating BY "
          "CONSTRUCTION (no fixture can fix them)" % blind)
    indep = sum(1 for r in rows if r.get("input_independent") is True)
    known = sum(1 for r in rows if r.get("input_independent") is not None)
    print("PORT gates whose Python fn never reads its input: %d of %d resolvable "
          "(%d PORT total) -- these cannot discriminate, no fixture can fix them"
          % (indep, known, counts.get("PORT", 0)))
    tup = sum(1 for r in rows if r.get("returns_tuple") is True)
    print("PORT gates whose Python fn returns a TUPLE: %d -- the generated twin "
          "mixes str(tuple) while the harness mixes the scalar, so these can never "
          "match until both sides are rewritten" % tup)
    na = counts.get("NA", 0)
    if na:
        print("%d gate(s) declared N/A with a reason (cannot discriminate or "
              "Python-only) -- excluded from PORT by declaration" % na)
    print("PORT is parity evidence; COPY measures a hand transcription; "
          "SHIM measures the gate. Only COPY->PORT conversion changes what a green means.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
