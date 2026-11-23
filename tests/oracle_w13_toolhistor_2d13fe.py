#!/usr/bin/env python3
"""Differential oracle for agent/tool_history_as_text.py (gate w13_toolhistor_2d13fe)."""
import importlib.util
import json
import sys

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

Q = "C:/Users/eman5/hermes-agent"
G = "w13_toolhistor_2d13fe"
if Q not in sys.path:
    sys.path.insert(0, Q)

spec = importlib.util.spec_from_file_location(G + "_live", Q + "/agent/tool_history_as_text.py")
live = importlib.util.module_from_spec(spec)
sys.modules[G + "_live"] = live
spec.loader.exec_module(live)

_FUNCS = ("text_of", "history")


def live_out(rec):
    func = rec.get("func")
    if func == "text_of":
        return live._text_of(rec.get("content"))
    if func == "history":
        msgs = rec.get("messages")
        return live.tool_history_for_request(msgs if msgs is not None else [],
                                             rec.get("tools"), rec.get("tool_choice"))
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in _FUNCS:
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%s func=%s C=%r PY=%r" %
                  (G, rec.get("case"), rec.get("func"), got, py), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (G, cases, mism))
    return 1 if (mism or cases == 0) else 0


if __name__ == "__main__":
    sys.exit(main())
