#!/usr/bin/env python3
"""Oracle for the pure helpers in agent/auxiliary_call_outcome.py
(embedded_provider_error, is_cancellation) and gateway/platforms/event.py
(clean_attachment_filename, _command_text). The functions are ast-extracted and
run live with their module globals seeded.
"""
import ast
import json
import re
import sys
import unicodedata
from types import SimpleNamespace

SRC_AUX = "C:/Users/eman5/hermes-agent/agent/auxiliary_call_outcome.py"
SRC_EV = "C:/Users/eman5/hermes-agent/gateway/platforms/event.py"

NS = {}


def _collect(path, names):
    code = open(path, encoding="utf-8").read()
    tree = ast.parse(code)
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in names:
            seg = ast.get_source_segment(code, n)
            exec(compile(ast.Module(body=[ast.parse(seg).body[0]],
                                    type_ignores=[]), "x", "exec"), NS)
        elif isinstance(n, ast.ClassDef):
            for m in n.body:
                if isinstance(m, ast.FunctionDef) and m.name in names:
                    seg = ast.get_source_segment(code, m)
                    seg = "\n".join(l for l in seg.splitlines()
                                    if not l.strip().startswith("@"))
                    exec(compile(ast.Module(body=[ast.parse(seg).body[0]],
                                            type_ignores=[]), "x", "exec"), NS)


_collect(SRC_AUX, {"embedded_provider_error", "is_cancellation"})
_collect(SRC_EV, {"clean_attachment_filename", "_command_text"})
NS["unicodedata"] = unicodedata
NS["_MAX_FILENAME_CHARS"] = 255
NS["_ATTACHMENT_REF_RE"] = re.compile(r"^(?:@(?:image|file|url):[^\n]+\n?)+",
                                     re.IGNORECASE)
NS["re"] = re

caf = NS["clean_attachment_filename"]
ct_ = NS["_command_text"]
epe = NS["embedded_provider_error"]
ic_ = NS["is_cancellation"]


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
        print("ORACLE aux_outcome_event: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "caf":
            want = caf(c["name"])
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH caf {c['case']}: C={c['out']!r} PY={want!r}")
        elif fam == "ct":
            me = SimpleNamespace(text=c["text"])
            want = ct_(me)
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH ct {c['case']}: C={c['out']!r} PY={want!r} "
                      f"text={c['text']!r}")
        elif fam == "epe":
            want = epe(c["response"])
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH epe {c['case']}: C={c['out']!r} PY={want!r}")
        elif fam == "ic":
            err = __import__("builtins").InterruptedError() if c["intr"] else ValueError()
            if not c["exc"]:
                err = 5
            want = 1 if ic_(err) else 0
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH ic {c['case']}: C={c['out']} PY={want}")
    print(f"ORACLE aux_outcome_event: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
