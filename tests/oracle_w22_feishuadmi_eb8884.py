#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/feishu/feishu_admission_diagnostics.py
(gate w22_feishuadmi_eb8884).

The module is pure: one function, one decision. The live FunctionDef is exec'd
out of the AST against the quarry's own globals and called with the fixture's
keyword arguments, so the warning text and every branch are the real ones.

  warning -- live empty_allowlist_drop_warning(chat_id=..., group_rules=...,
             default_group_policy=..., allowed_group_users=...) -> str | None
"""
import ast
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "feishu" / "feishu_admission_diagnostics.py"
GATE = "w22_feishuadmi_eb8884"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

G = {}
_tree = ast.parse(SRC.read_text(encoding="utf-8"))
for _node in ast.walk(_tree):
    if isinstance(_node, (ast.FunctionDef, ast.AsyncFunctionDef)) and \
            _node.name == "empty_allowlist_drop_warning":
        _node.decorator_list = []
        exec(compile(ast.Module(body=[_node], type_ignores=[]), str(SRC), "exec"), G)
        break
else:  # pragma: no cover - the function must exist
    raise SystemExit("empty_allowlist_drop_warning not found in %s" % SRC)
empty_allowlist_drop_warning = G["empty_allowlist_drop_warning"]

WANTED = {"warning"}


def expected(r):
    f = r.get("func")
    if f == "warning":
        return empty_allowlist_drop_warning(
            chat_id=r.get("chat_id", ""),
            group_rules=r.get("group_rules", {}),
            default_group_policy=r.get("default_group_policy", "allowlist"),
            allowed_group_users=r.get("allowed_group_users", []),
        )
    raise AssertionError("unknown func %r" % (f,))


def check_isolation():
    """The runner isolates HOME/SLERMES_HOME to one temp dir: assert it, don't assume."""
    home = os.environ.get("HOME")
    slermes = os.environ.get("SLERMES_HOME")
    same = bool(home and slermes and
                os.path.normcase(os.path.abspath(home)) ==
                os.path.normcase(os.path.abspath(slermes)))
    if not same or not os.path.isdir(home):
        print("HARNESS-ENV not isolated: HOME=%r SLERMES_HOME=%r" % (home, slermes),
              file=sys.stderr)
        sys.exit(2)


def main():
    check_isolation()
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
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
