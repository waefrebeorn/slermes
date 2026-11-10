#!/usr/bin/env python3
"""Oracle for hermes_cli/auth_commands.py _display_source + _classify_exhausted_status.

Harness emits {"tag":"ds","source","out"} and
{"tag":"classify","code","reason","message","out"}. Ast-extracts the LIVE defs
(auth_commands has heavy imports) and drives them on the harness fields.
"""
import ast
import sys

sys.stdin.reconfigure(encoding="utf-8")
SRC = "C:/Users/eman5/hermes-agent/hermes_cli/auth_commands.py"

ns = {}
tree = ast.parse(open(SRC, encoding="utf-8").read())


def _extract(name):
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name == name:
            exec(compile(ast.Module(body=[n], type_ignores=[]), "<x>", "exec"), ns)
            return ns[name]
    raise KeyError(name)


# _classify_exhausted_status reads module global _EXHAUSTED_CLASSES at call time.
for n in tree.body:
    if isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name) and \
       n.targets[0].id == "_EXHAUSTED_CLASSES":
        exec(compile(ast.Module(body=[n], type_ignores=[]), "<c>", "exec"), ns)


_display_source = _extract("_display_source")
_classify = _extract("_classify_exhausted_status")


def _scalar(v):
    # a field that came from json (None kept as None, int kept as int)
    return v


passes = 0
fails = []
total = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    import json
    c = json.loads(line)
    tag = c["tag"]
    if tag == "ds":
        src = c["source"] or ""
        want = _display_source(src)
        total += 1
        if c["out"] == want:
            passes += 1
        else:
            fails.append(("ds", src, want, c["out"]))
    elif tag == "classify":
        code = c["code"] if c["code"] is not None else None
        if isinstance(code, str) and code.lstrip("-").isdigit():
            code = int(code)
        reason = c["reason"] or ""
        message = c["message"] or ""
        want = _classify((type("E", (), {"last_error_code": code,
                                        "last_error_reason": reason,
                                        "last_error_message": message})()))
        total += 1
        got = c["out"]
        # Python returns (label, bool); C emits "label,1"/"label,0"
        want_str = "%s,%d" % (want[0], 1 if want[1] else 0)
        if got == want_str:
            passes += 1
        else:
            fails.append(("classify", (code, reason, message), want_str, got))
    elif tag == "wait":
        remaining = c["remaining"]
        minutes, seconds = divmod(remaining, 60)
        hours, minutes = divmod(minutes, 60)
        days, hours = divmod(hours, 24)
        parts = [(days, "d"), (hours, "h"), (minutes, "m"), (seconds, "s")]
        first = next(i for i, (value, _) in enumerate(parts) if value or i == 3)
        want = " ".join("%d%s" % (value, unit)
                        for value, unit in parts[first:first + 2])
        total += 1
        if c["out"] == want:
            passes += 1
        else:
            fails.append(("wait", remaining, want, c["out"]))

print("pass/total: %d/%d" % (passes, total))
for f in fails[:10]:
    print("FAIL", f[0], "in=", f[1], "want=", f[2], "c=", f[3])
if fails:
    print("ORACLE auth_commands: %d cases, %d mismatches" % (total, len(fails)))
    sys.exit(1)
print("ORACLE auth_commands: %d cases, 0 mismatches" % total)
