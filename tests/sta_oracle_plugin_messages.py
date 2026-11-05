#!/usr/bin/env python3
"""Oracle for gateway/plugin_messages.py _err, _fit, _supports_edit.
Ast-extracted and run live; the adapter measure is stubbed to len.
"""
import ast
import json
import sys
from types import SimpleNamespace

SRC = "C:/Users/eman5/hermes-agent/gateway/plugin_messages.py"
code = open(SRC, encoding="utf-8").read()
tree = ast.parse(code)

NS = {}
for n in tree.body:
    if isinstance(n, ast.FunctionDef) and n.name in ("_err", "_fit", "_supports_edit"):
        seg = ast.get_source_segment(code, n)
        exec(compile(ast.Module(body=[ast.parse(seg).body[0]],
                                type_ignores=[]), "x", "exec"), NS)

err_ = NS["_err"]
fit_ = NS["_fit"]
sup_ = NS["_supports_edit"]


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
        print("ORACLE plugin_messages: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "err":
            want = err_(c["code"], c["detail"])
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH err {c['case']}: C={c['out']} PY={want}")
        elif fam == "fit":
            mock = SimpleNamespace(
                max_message_length_for_chat=lambda cid: c["limit"],
                message_len_fn_for_chat=lambda cid: len)
            want = fit_(mock, "x", c["text"])
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH fit {c['case']}: C={c['out']!r} PY={want!r} "
                      f"text={c['text']!r} limit={c['limit']}")
        elif fam == "supp":
            # _supports_edit: editing flag False -> False, else has_custom_edit
            want = 1 if (c["flag"] and c["custom"]) else 0
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH supp {c['case']}: C={c['out']} PY={want}")
    print(f"ORACLE plugin_messages: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
