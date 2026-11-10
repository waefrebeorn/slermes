#!/usr/bin/env python3
"""Oracle for agent/context_compressor_telemetry.py pure helpers:
_compression_method and the aux-route failure error truncation. Both are
ast-extracted from the mixin class and run live on a stub self. fit_margin is
the exact one-line formula (no free terms).
"""
import ast
import json
import sys
from types import SimpleNamespace

SRC = "C:/Users/eman5/hermes-agent/agent/context_compressor_telemetry.py"
code = open(SRC, encoding="utf-8").read()
tree = ast.parse(code)

NS = {}
for n in tree.body:
    if isinstance(n, ast.ClassDef) and n.name == "CompressionTelemetryMixin":
        for m in n.body:
            if isinstance(m, ast.FunctionDef) and m.name in (
                    "_compression_method", "_note_aux_route_fallback"):
                seg = ast.get_source_segment(code, m)
                fn = ast.parse(seg).body[0]
                exec(compile(ast.Module(body=[fn], type_ignores=[]), "x", "exec"), NS)

_method = NS["_compression_method"]
_note = NS["_note_aux_route_fallback"]


def expected_clamp(failure):
    if failure is None:
        return None
    if len(failure) <= 220:
        return failure
    return failure[:217].rstrip() + "..."


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE comp_telemetry: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "method":
            self_ = SimpleNamespace(
                _last_summary_fallback_used=bool(c["fallback"]),
                _last_aux_model_failure_error="E" if c["auxerr"] else None)
            want = _method(self_)
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH method {c['case']}: C={c['out']!r} PY={want!r}")
        elif fam == "clamp":
            self_ = SimpleNamespace(_active_compression_telemetry=None,
                                    _last_aux_model_failure_model=None,
                                    _last_aux_model_failure_error=None)
            route = {"failed_model": "aux", "fallback_label": "main-agent(x)",
                     "failure": c["f"],
                     "__found": False}
            _note(self_, route)
            want = self_._last_aux_model_failure_error
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH clamp {c['case']}: C={c['out']!r} PY={want!r} "
                      f"(len={len(c['f'] or '')})")
        elif fam == "margin":
            want = c["eff"] - c["prompt"] - (c["resv"] if c["resvpres"] else 0)
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH margin {c['case']}: C={c['out']} PY={want}")
    print(f"ORACLE comp_telemetry: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
