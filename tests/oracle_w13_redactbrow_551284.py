#!/usr/bin/env python3
"""Differential oracle for agent/redact_browser_input.py (w13_redactbrow_551284).

The AST-parsed path has no C analogue, so `ast.parse` is forced to raise with a
seam: the live module always takes its documented fail-closed raw-text branch
(_unparsed_masks + .value-assignment masks). Every ported function reproduces
the module's fixed regex scans on the same code string.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/agent/redact_browser_input.py"
spec = importlib.util.spec_from_file_location("redact_browser_input_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["redact_browser_input_live"] = live
spec.loader.exec_module(live)


def _force_parse(*a, **k):
    raise SyntaxError("forced unparsed branch")


live.ast = types.SimpleNamespace(parse=_force_parse)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    code = r.get("code", "")
    try:
        if func == "literal_end":
            lit = live._literal_end(code, int(r.get("start", 0)))
            py = None if lit is None else {"prefix": lit[0], "quote": lit[1], "end": lit[2]}
        elif func == "skip_comment":
            py = live._skip_comment(code, int(r.get("i", 0)))
        elif func == "literal_mask":
            m = live._literal_mask(code, int(r.get("start", 0)))
            py = None if m is None else list(m)
        elif func == "call_end":
            py = live._call_end(code, int(r.get("i", 0)))
        elif func == "literal_regions":
            py = [list(x) for x in live._literal_regions(code, int(r.get("start", 0)),
                                                        int(r.get("end", 0)))]
        elif func == "unparsed_masks":
            py = [list(x) for x in live._unparsed_masks(code)]
        elif func == "mask_unparsed":
            py = live.mask_typed_browser_text(code)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} case={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w13_redactbrow_551284: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
