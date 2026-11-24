#!/usr/bin/env python3
"""Differential oracle for tools/approval_parser_budget.py (gate w16_approvalpa_cd9d4d)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w16_approvalpa_cd9d4d"
sys.path.insert(0, QUARRY)

# Parent-package bootstrap: the live module imports tools.approval_ansi_c.
_pkg = types.ModuleType("tools")
_pkg.__path__ = [QUARRY + "/tools"]
sys.modules["tools"] = _pkg


def load(name, rel):
    spec = importlib.util.spec_from_file_location(name, QUARRY + "/" + rel)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


# Register the real ANSI-C helper before the budget module imports it.
load("tools.approval_ansi_c", "tools/approval_ansi_c.py")
live = load("tools.approval_parser_budget", "tools/approval_parser_budget.py")


def live_out(rec):
    func = rec.get("func")
    if func == "substitution_depth":
        return live.substitution_depth(rec.get("command") or "")
    if func == "scan_shell":
        o = rec.get("opts") or {}
        end = o.get("end")
        steps = live._scan_shell(
            o.get("text") or "", int(o.get("start", 0)),
            None if end is None else int(end),
            subst=o.get("subst", ""), brace=bool(o.get("brace", False)),
            stop_unterminated=bool(o.get("stop_unterminated", False)),
            naive_backtick=bool(o.get("naive_backtick", False)),
            comments=bool(o.get("comments", False)),
        )
        return [[k, i, j, q] for (k, i, j, q) in steps]
    if func == "dollar_paren_end":
        return live._scan_dollar_paren_end(rec.get("command") or "", int(rec.get("start", 0)))
    if func == "backtick_end":
        return live._scan_backtick_end(rec.get("command") or "", int(rec.get("start", 0)))
    if func == "heredoc_bodies":
        pending = [tuple(p) for p in (rec.get("pending") or [])]
        spans = live._quoted_heredoc_body_spans(rec.get("command") or "", int(rec.get("newline", 0)), pending)
        return [[a, b] for a, b in spans]
    return None


def main():
    cases = mism = 0
    allowed = ("substitution_depth", "scan_shell", "dollar_paren_end", "backtick_end", "heredoc_bodies")
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
