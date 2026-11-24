#!/usr/bin/env python3
"""Oracle for gate w18_tuiworktre_fbaffe (hermes_cli/tui_worktree_resume.py).

Ast-extracts the two pure defs (_key, _recorded_tree_for) from the live module.
_read() and Path.exists() are the filesystem seams and are injected from the
fixture; everything else is the live code verbatim.
"""
import ast
import json
import os
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
SRC = QUARRY + "/hermes_cli/tui_worktree_resume.py"

EXISTING = frozenset()


def norm_key(s):
    return os.path.normcase(os.path.realpath(str(s)))


class Path:
    """Pure subset of pathlib.Path used by _recorded_tree_for."""
    def __init__(self, s):
        self.s = str(s)

    def __str__(self):
        return self.s

    def __truediv__(self, other):
        return Path(self.s.rstrip('/\\') + '/' + str(other))

    @property
    def parent(self):
        s = self.s.rstrip('/\\')
        idx = -1
        for i, ch in enumerate(s):
            if ch in '/\\':
                idx = i
        if idx < 0:
            return Path(s)
        if idx <= 2 and len(s) > 1 and s[1] == ':':
            return Path(s[:idx + 1])
        if idx == 0:
            return Path('\\')
        return Path(s[:idx])

    @property
    def parents(self):
        out, cur, seen = [], self.parent, set()
        while str(cur) not in seen and str(cur) != self.s:
            seen.add(str(cur))
            out.append(cur)
            cur = cur.parent
        return out

    def exists(self):
        return norm_key(self.s) in EXISTING


def extract():
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    ns = {"os": os, "Path": Path, "Optional": object, "str": str,
          "dict": dict, "isinstance": isinstance, "_read": lambda: {}}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in ("_key", "_recorded_tree_for"):
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


NS = extract()


def main():
    global EXISTING
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
        try:
            if func == "key":
                py = NS["_key"](r.get("path", ""))
            elif func == "recorded":
                EXISTING = frozenset(norm_key(p) for p in (r.get("existing") or []))
                NS["_read"] = lambda: (r.get("trees") or {})
                py = NS["_recorded_tree_for"](r.get("cwd", ""))
            else:
                continue
        except Exception as exc:
            print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
            continue
        cases += 1
        got = r.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
    print(f"ORACLE w18_tuiworktre_fbaffe: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
