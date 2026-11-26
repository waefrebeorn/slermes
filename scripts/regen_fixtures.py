#!/usr/bin/env python3
"""Regenerate a gate's fixtures so its cups can move.

The generated suite feeds every gate the same five words -- hello / world / "" / test123 / NULL -- so
a gate over floats answers "false" five times, a gate over ints answers "0" five times, and the run
reports NON-DISCRIMINATING: the fixtures never reach the branch under test. A gate that cannot move
its answer cannot prove parity, no matter how many cups match.

This rewrites `inputs` from the annotation of the Python function the twin calls, using a corpus that
is meant to straddle the boundaries (numeric text vs not, empty vs set, long vs short). It does not
touch the port, the harness, or the twin: only the data the gate is driven with.

    python scripts/regen_fixtures.py --dry-run --limit 20
    python scripts/regen_fixtures.py --only agent_account_usage_is_finite_num
"""

from __future__ import annotations

import argparse
import importlib
import importlib.util
import inspect
import json
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
GATES = REPO / "tests" / "watercup" / "gates"
_IMPORT_ERRORS: dict[str, str] = {}
QUARRY = pathlib.Path("C:/Users/eman5/hermes-agent")

# Per-annotation corpora. Every entry is text the harness can pass on argv, and "NULL" is the
# harness's spelling of a null pointer.
CORPUS = {
    "float": ["0.0", "1.5", "-2.5", "1024", "hello"],
    "int": ["0", "1", "-2", "1024", "hello"],
    "bool": ["1", "0", "true", "false", "NULL"],
    # A str corpus has to contain shapes, not just words: a row/table predicate cannot answer
    # anything but false for "hello", so the gate never discriminates.
    "str": ["hello", "| a | b |", "", "a_b-c", "NULL"],
    "any": ["0", "hello", "", "1.5", "NULL"],
}


def _kind(annotation) -> str:
    a = str(annotation).lower() if annotation is not inspect.Parameter.empty else "any"
    if "bool" in a:
        return "bool"
    if "float" in a or "double" in a:
        return "float"
    if "int" in a and "point" not in a and "print" not in a:
        return "int"
    if "str" in a or "path" in a:
        return "str"
    return "any"


def _live_fn(gate: dict):
    """Import the twin's target function from the quarry, or None.

    Import it as a *package* module: these files do `from agent.x import y`, so loading the file by
    path fails and every gate then looked like "no annotation".
    """
    mod = (gate.get("py_module") or "").replace("\\", "/")
    fn = gate.get("py_fn") or ""
    if not mod.endswith(".py") or not fn:
        return None
    dotted = mod[:-3].replace("/", ".")
    if str(QUARRY) not in sys.path:
        sys.path.insert(0, str(QUARRY))
    try:
        m = importlib.import_module(dotted)
        return getattr(m, fn, None)
    except Exception as exc:  # noqa: BLE001
        _IMPORT_ERRORS[gate.get("name") or dotted] = f"{type(exc).__name__}: {exc}"
        return None




_FN_INDEX_CACHE = REPO / ".testbin" / "fn_index.json"


def _fn_index() -> dict:
    """fn name -> module path, built in ONE pass over the quarry (a per-gate walk took minutes)."""
    try:
        blob = json.loads(_FN_INDEX_CACHE.read_text(encoding="utf-8"))
        if blob.get("generated_from") == str(QUARRY):
            return blob["index"]
    except (OSError, ValueError):
        pass
    index: dict[str, str] = {}
    pat = re.compile(r"^\s*(?:async\s+)?def\s+([A-Za-z_]\w*)\s*\(", re.M)
    # Bound the walk: the package trees these fns live in, and no file over 1 MB. A full-recursive
    # read of the quarry took minutes.
    roots = [QUARRY / d for d in ("agent", "tools", "session_rules", "gateway", "plugins",
                                  "cli", "acp_adapter", "browser", "cron", "infrastructure")
             if (QUARRY / d).is_dir()]
    seen = 0
    for root in roots:
        for cand in root.rglob("*.py"):
            parts = cand.parts
            if any(x in parts for x in ("tests", ".venv", "node_modules", "site-packages", "__pycache__")):
                continue
            try:
                if cand.stat().st_size > 1_000_000:
                    continue
                text = cand.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            seen += 1
        rel = str(cand.relative_to(QUARRY)).replace("\\", "/")
        for m in pat.finditer(text):
            index.setdefault(m.group(1), rel)
    _FN_INDEX_CACHE.parent.mkdir(parents=True, exist_ok=True)
    _FN_INDEX_CACHE.write_text(json.dumps({"generated_from": str(QUARRY), "index": index}),
                               encoding="utf-8", newline="")
    return index


def _resolve_module(gate: dict) -> str:
    """The module that actually defines the fn.

    The generator guessed a file from the gate name, so `_fmt_usd` was attributed to
    agent/account_usage.py while it lives in agent/account_usage_nous.py -- the twin then imported a
    module that has no such attribute and the gate could never be traced. Look for the definition
    when the declared module does not have it.
    """
    declared = (gate.get("py_module") or "").replace("\\", "/")
    fn = gate.get("py_fn") or ""
    if not fn:
        return declared
    if declared.endswith(".py") and (QUARRY / declared).exists():
        try:
            if str(QUARRY) not in sys.path:
                sys.path.insert(0, str(QUARRY))
            if getattr(importlib.import_module(declared[:-3].replace("/", ".")), fn, None):
                return declared
        except Exception:
            pass
    found = _fn_index().get(fn)
    if found:
        return found
    return declared


def corpus_for(fn) -> list[str]:
    if fn is None:
        return []
    try:
        params = list(inspect.signature(fn).parameters.values())
    except (TypeError, ValueError):
        return []
    if not params:
        return []
    return CORPUS[_kind(params[0].annotation)]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--only", nargs="*", default=[])
    ap.add_argument("--only-non-discriminating", default="",
                    help="path to a report listing NON-DISCRIMINATING gate names")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    targets = {g.name: g for g in sorted(GATES.iterdir())}
    names = args.only or list(targets)
    if args.only_non_discriminating:
        text = pathlib.Path(args.only_non_discriminating).read_text(encoding="utf-8", errors="replace")
        names = [n for n in names if n in text]
    if args.limit:
        names = names[: args.limit]

    changed = 0
    skipped = []
    for name in names:
        gd = targets.get(name)
        if gd is None:
            continue
        gj = gd / "gate.json"
        if not gj.exists():
            continue
        gate = json.loads(gj.read_text(encoding="utf-8"))
        gate.setdefault("name", name)
        fixed_mod = _resolve_module(gate)
        if fixed_mod and fixed_mod != (gate.get("py_module") or ""):
            print(f"{name}: py_module {gate.get('py_module')} -> {fixed_mod}")
            gate["py_module"] = fixed_mod
            # The twin carries its own copy of the module name -- fixing only gate.json left the
            # twin importing the wrong file and answering one constant ERROR cup.
            tw = gd / "twin.py"
            if tw.exists() and not args.dry_run:
                ts = tw.read_text(encoding="utf-8")
                ts2 = re.sub(r'PY_MODULE\s*=\s*"[^"]*"',
                             'PY_MODULE = "%s"' % fixed_mod, ts, count=1)
                if ts2 != ts:
                    tw.write_text(ts2, encoding="utf-8", newline='')
        fn = _live_fn(gate)
        corpus = corpus_for(fn)
        if not corpus:
            skipped.append((name, "no annotation / fn not importable"))
            continue
        # A twin that still names the wrong module is fixed even when the fixtures are already fine.
        tw = gd / "twin.py"
        if tw.exists() and gate.get("py_module"):
            ts = tw.read_text(encoding="utf-8")
            ts2 = re.sub(r'PY_MODULE\s*=\s*"[^"]*"', 'PY_MODULE = "%s"' % gate["py_module"], ts, count=1)
            if ts2 != ts and not args.dry_run:
                tw.write_text(ts2, encoding="utf-8", newline='')
                print(f"{name}: twin PY_MODULE aligned to {gate['py_module']}")

        if gate.get("inputs") == corpus:
            skipped.append((name, "already uses this corpus"))
            continue
        print(f"{name}: {gate.get('inputs')} -> {corpus}")
        gate["inputs"] = corpus
        if not args.dry_run:
            gj.write_text(json.dumps(gate, indent=2) + "\n", encoding="utf-8", newline="")
        changed += 1

    print(f"\n{changed} gate(s) rewritten, {len(skipped)} skipped")
    for n, why in skipped[:10]:
        print(f"  skip {n}: {why}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
