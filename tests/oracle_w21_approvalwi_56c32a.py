#!/usr/bin/env python3
"""Differential oracle for tools/approval_windows_hardline.py
(gate w21_approvalwi_56c32a).

The live module is pure `re` + string concatenation, so it is imported from its
real source with nothing stubbed: the pattern builders are compared text for
text, and `whatif_preview_is_real` is the live `re.search`/`re.split` verdict
that the C side must match through PCRE2.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

GATE = "w21_approvalwi_56c32a"
SRC = "C:/Users/eman5/hermes-agent/tools/approval_windows_hardline.py"

_sp = importlib.util.spec_from_file_location(GATE + "_live", SRC)
live = importlib.util.module_from_spec(_sp)
sys.modules[GATE + "_live"] = live
_sp.loader.exec_module(live)


def expected(r):
    f = r.get("func")
    if f == "hardline_patterns":
        return [[pattern, desc] for pattern, desc in live.windows_hardline_patterns(r.get("cmdpos", ""))]
    if f == "strict_patterns":
        return dict(live.windows_whatif_strict_patterns(r.get("cmdpos", "")))
    if f == "win_cmdpos":
        return live._win_cmdpos(r.get("cmdpos", ""))
    if f == "whatif":
        return live.whatif_preview_is_real(r.get("command", ""))
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"hardline_patterns", "strict_patterns", "win_cmdpos", "whatif"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
