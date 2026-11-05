#!/usr/bin/env python3
"""Oracle for gateway/turn_lease.py formatting helpers: _holder_desc,
TurnLeaseToken.__repr__, and the TurnLeaseTimeoutError message. All three are
run live from the ast-extracted source.
"""
import ast
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/gateway/turn_lease.py"
code = open(SRC, encoding="utf-8").read()
tree = ast.parse(code)

holder = repr_ = timeout_cls = None
for n in tree.body:
    if isinstance(n, ast.FunctionDef) and n.name == "_holder_desc":
        holder = ast.get_source_segment(code, n)
    elif isinstance(n, ast.ClassDef) and n.name == "TurnLeaseToken":
        for m in n.body:
            if isinstance(m, ast.FunctionDef) and m.name == "__repr__":
                repr_ = ast.get_source_segment(code, m)
    elif isinstance(n, ast.ClassDef) and n.name == "TurnLeaseTimeoutError":
        timeout_cls = ast.get_source_segment(code, n)

NS = {}
for seg in (holder, repr_):
    if seg:
        exec(compile(ast.Module(body=[ast.parse(seg).body[0]], type_ignores=[]), "x", "exec"), NS)
if timeout_cls:
    exec(compile(ast.parse(timeout_cls), "x", "exec"), NS)


class _Tok:
    def __init__(self, sid, key, gen, rel):
        self.session_id, self.owner_key = sid, key
        self.generation, self.released = gen, rel


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
        print("ORACLE turn_lease: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "holder":
            if c["present"]:
                want = [c["key"], c["gen"]]
            else:
                want = ["?", "?"]
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH holder {c['case']}: C={c['out']} PY={want}")
        elif fam == "repr":
            tok = _Tok(c["sid"], c["key"], c["gen"], bool(c["rel"]))
            want = NS["__repr__"](tok)
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH repr {c['case']}: C={c['out']!r} PY={want!r}")
        elif fam == "tout":
            exc = NS["TurnLeaseTimeoutError"](
                c["sid"], owner_key=c["key"], generation=c["gen"],
                wait_seconds=c["wait"])
            want = str(exc)
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH tout {c['case']}: C={c['out']!r} PY={want!r}")
    print(f"ORACLE turn_lease: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
