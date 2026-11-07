#!/usr/bin/env python3
"""Oracle for agent/context_compressor_telemetry.py pure helpers
(port include/port_comp_telemetry.h):
  method:  _compression_method(fallback_used, has_aux_failure_error)
  clamp:   truncation in _note_aux_route_fallback: failure if len<=220 else
           failure[:217].rstrip() + "..."
  margin:  fit_margin = eff - prompt - (resv or 0)   (None resv -> 0)
The live helpers are mixin methods reading self._last_summary_fallback_used /
self._last_aux_model_failure_error, and inline truncation/margin math — the
harness drives the pure (parametrized) C ports, so the oracle ast-extracts
_compression_method, binds a tiny fake self, and mirrors clamp/margin."""
import ast
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def _clamp_failure(f):
    if f is None:
        return ""
    return f if len(f) <= 220 else f[:217].rstrip() + "..."


def main() -> int:
    src = open("C:/Users/eman5/hermes-agent/agent/context_compressor_telemetry.py",
               encoding="utf-8").read()
    mns = {}
    tree = ast.parse(src)
    for n in tree.body:
        if isinstance(n, ast.ClassDef):
            for m in n.body:
                if isinstance(m, ast.FunctionDef) and m.name == "_compression_method":
                    exec(compile(ast.Module(body=[m], type_ignores=[]), "cm", "exec"), mns)
    cm = mns["_compression_method"]
    total, mism = 0, 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            import json
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fam = c.get("family")
        if fam == "method":
            fake = type("S", (), {
                "_last_summary_fallback_used": bool(c.get("fallback")),
                "_last_aux_model_failure_error": "e" if c.get("auxerr") else None,
            })()
            want = cm(fake)
            if want != c.get("out"):
                mism += 1
                print(f"MISMATCH method {c.get('case')} C={c.get('out')} PY={want}")
        elif fam == "clamp":
            want = _clamp_failure(c.get("f"))
            if want != c.get("out"):
                mism += 1
                print(f"MISMATCH clamp {c.get('case')} C={c.get('out')!r} PY={want!r}")
        elif fam == "margin":
            resv = c.get("resv") or 0
            want = c.get("eff") - c.get("prompt") - resv
            if want != c.get("out"):
                mism += 1
                print(f"MISMATCH margin {c.get('case')} C={c.get('out')} PY={want}")
    if total == 0:
        print("ORACLE comp_telemetry: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE comp_telemetry: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
