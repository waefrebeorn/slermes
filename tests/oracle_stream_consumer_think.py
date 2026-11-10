#!/usr/bin/env python3
"""Oracle for gateway/stream_consumer_think.py StreamThinkFilterMixin
_at_block_boundary + _earliest_open_tag.

Ast-extracts the LIVE class, drives both methods on a fake instance with the
real THINK_OPEN_TAGS and per-case _accumulated, and compares to the C output.
"""
import ast
import sys

sys.stdin.reconfigure(encoding="utf-8")
SRC = "C:/Users/eman5/hermes-agent/gateway/stream_consumer_think.py"

THINK_OPEN_TAGS = (
    " thinking", "<thinking>", "<reasoning>", "<thought>",
    "<reasoning_scratchpad>",
    "<\u601d\u8003>", "<\u53cd\u601d>", "<\u63a8\u7406>", "<\u63a8\u6572>",
)


def _extract_class(path, name):
    source = open(path, encoding="utf-8").read()
    tree = ast.parse(source)
    # seed the module globals the class body references
    for n in tree.body:
        if isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name) and \
           n.targets[0].id == "_OPEN_THINK_TAGS":
            exec(compile(ast.Module(body=[n]), "<c>", "exec"), ns)
    for n in tree.body:
        if isinstance(n, ast.ClassDef) and n.name == name:
            exec(compile(ast.Module(body=[n]), "<x>", "exec"), ns)
            return ns[name]
    raise KeyError(name)


ns = {"THINK_OPEN_TAGS": THINK_OPEN_TAGS, "THINK_CLOSE_TAGS": None}
Cls = _extract_class(SRC, "StreamThinkFilterMixin")


class _Fake:
    pass


passes = 0
fails = []
total = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    s = line
    case = {}
    parts = s[1:-1]
    # minimal parse: pull known fields
    import json
    case = json.loads(s)
    tag = case["tag"]
    obj = _Fake()
    obj._accumulated = case.get("acc") or ""
    obj._OPEN_THINK_TAGS = THINK_OPEN_TAGS
    obj._at_block_boundary = Cls._at_block_boundary.__get__(obj)
    obj._earliest_open_tag = Cls._earliest_open_tag.__get__(obj)
    if tag == "bd":
        buf = case["buf"]
        idx = case["idx"]
        out = 1 if Cls._at_block_boundary(obj, buf, idx) else 0
        c = case.get("out", 0)
        total += 1
        if out == c:
            passes += 1
        else:
            fails.append(("bd", (buf, idx), out, c))
    elif tag == "ot":
        buf = case["buf"]
        lb = case["lower"]
        idx, ln = Cls._earliest_open_tag(obj, buf, lb)
        c_found = bool(case.get("found"))
        c_idx = case.get("idx")
        c_len = case.get("len")
        ok = (idx == c_idx and ln == c_len and
              (idx >= 0) == c_found and (ln > 0) == c_found)
        total += 1
        if ok:
            passes += 1
        else:
            fails.append(("ot", buf, (idx, ln, idx >= 0), (c_idx, c_len, c_found)))

print("pass/total: %d/%d" % (passes, total))
for f in fails[:10]:
    print("FAIL", f[0], "buf=", f[1], "py=", f[2], "c=", f[3])
if fails:
    print("ORACLE stream_consumer_think: %d cases, %d mismatches" % (total, len(fails)))
    sys.exit(1)
print("ORACLE stream_consumer_think: %d cases, 0 mismatches" % total)
