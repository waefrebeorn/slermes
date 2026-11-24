#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/telegram/telegram_media_budget.py
(gate w21_telegramme_52d000).

The live module is executed from its real source: `media_sources` and
`scaled_budgets` are lifted out of the AST together with the module constants
(`_MEDIA_KEYS`, `MEDIA_READ_TIMEOUT`, `MEDIA_WRITE_TIMEOUT`), and the two
gateway.platforms.helpers functions it calls (`media_upload_seconds`,
`scaled_upload_deadline`) are lifted from the real helpers.py the same way.

The single stubbed seam is `_on_disk_size` (helpers.py) -- it is os.stat on a
path / os.fstat on an open handle, i.e. the filesystem. The oracle replaces it
with the fixture's resolved byte count, so both sides sum the same numbers.
"""
from __future__ import annotations
import __future__
import ast
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "telegram" / "telegram_media_budget.py"
HELPERS = QUARRY / "gateway" / "platforms" / "helpers.py"
GATE = "w21_telegramme_52d000"
_ANN = __future__.annotations.compiler_flag


def _exec(path, names, into):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names:
            node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                         flags=_ANN, dont_inherit=True), into)
        elif isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            if any(isinstance(t, ast.Name) and t.id in names for t in targets):
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                             flags=_ANN, dont_inherit=True), into)


H = {}
_exec(HELPERS, {"media_upload_seconds", "scaled_upload_deadline",
                "MIN_MEDIA_UPLOAD_RATE", "MEDIA_UPLOAD_DEADLINE_MAX"}, H)
# filesystem seam: the fixture carries the already-resolved on-disk byte count
H["_on_disk_size"] = lambda value: value if isinstance(value, int) and not isinstance(value, bool) else 0

G = {"media_upload_seconds": H["media_upload_seconds"],
     "scaled_upload_deadline": H["scaled_upload_deadline"]}
_exec(SRC, {"media_sources", "scaled_budgets", "_MEDIA_KEYS",
            "MEDIA_READ_TIMEOUT", "MEDIA_WRITE_TIMEOUT"}, G)


def expected(r):
    f = r.get("func")
    if f == "sources":
        return G["media_sources"](r.get("media") or {})
    if f == "seconds":
        return G["media_upload_seconds"](r.get("sizes") or [])
    if f == "deadline":
        return G["scaled_upload_deadline"](r.get("deadline"), r.get("seconds"))
    if f == "budgets":
        timeouts, deadline = G["scaled_budgets"](
            r.get("sizes") or [], r.get("deadline"), r.get("read_timeout", G["MEDIA_READ_TIMEOUT"]))
        return {"timeouts": timeouts, "deadline": deadline}
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"sources", "seconds", "deadline", "budgets"}
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
        py = expected(r)
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
