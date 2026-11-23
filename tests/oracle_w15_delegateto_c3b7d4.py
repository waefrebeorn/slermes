#!/usr/bin/env python3
"""Differential oracle for tools/delegate_tool_results.py (gate w15_delegateto_c3b7d4)."""
import importlib.util
import json
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w15_delegateto_c3b7d4"
sys.path.insert(0, QUARRY)

# delegate_tool_results imports tools.delegate_tool_config at module load; the two
# ported helpers only need stdlib, so stub that config module.
import types
_to = types.ModuleType("tools")
_to.__path__ = []
sys.modules["tools"] = _to
_dtc = types.ModuleType("tools.delegate_tool_config")
sys.modules["tools.delegate_tool_config"] = _dtc
_dr = types.ModuleType("tools.delegation_redaction")
for _n in ("parent_copy", "redact_child_result", "redact_child_text", "redact_child_value", "redact_summary"):
    setattr(_dr, _n, lambda *a, **k: None)
sys.modules["tools.delegation_redaction"] = _dr

SRC = QUARRY + "/tools/delegate_tool_results.py"
_spec = importlib.util.spec_from_file_location("delegate_tool_results_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["delegate_tool_results_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "stringify":
        return live._stringify_tool_content(rec.get("content"))
    if func == "looks_error":
        return live._looks_like_error_output(rec.get("content"))
    return None


def main():
    cases = mism = 0
    allowed = ("stringify", "looks_error")
    for line in sys.stdin:
        line = line.strip()
        if not line or "{" not in line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in allowed:
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
