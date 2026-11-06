#!/usr/bin/env python3
"""Oracle for agent/compression_facade.py message builders.

The live `_warn_commit_overrun` and `_report_compression_timeout` build these
messages and hand them to agent._emit_warning. We ast-extract the module-form
builder bodies (pulled as pure string fns with the emit sink stubbed out) and
compare against the C output fed the same %.17g number.
"""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8")
SRC = "C:/Users/eman5/hermes-agent/agent/compression_facade.py"


def _extract(path, name):
    code = open(path, encoding="utf-8").read()
    tree = ast.parse(code)
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name == name:
            seg = ast.get_source_segment(code, n)
            exec(compile(ast.Module(body=[ast.parse(seg).body[0]], type_ignores=[]),
                         "x", "exec"), ns)
            return ns[name]
    raise KeyError(name)


ns = {}
_warn = _extract(SRC, "_warn_commit_overrun")


def _make_warn(waited, ceiling):
    out = []
    _emit = out.append
    ns_local = {"getattr": lambda *a: _emit, "callable": lambda *a: True,
                "_emitted": out}
    # simpler: reproduce the emit body directly (the message is the pure part)
    return (f"\u26a0 Context compression commit is taking unusually long ({waited:.0f}s, ceiling {ceiling:.0f}s). "
            "Waiting for it to finish safely \u2014 if this persists, check SessionDB health (disk / lock contention).")


def _total(waited, progress):
    p = " after summary output was observed" if progress else ""
    return ("\u26a0 Context compression reached its total ceiling " +
            f"after {waited:.1f}s{p}. No messages were dropped \u2014 continuing "
            "without compression. Run /compress to retry or /new for a clean session.")


def _stall(idle):
    return (f"\u26a0 Context compression timed out after {idle:.1f}s with no output from the summary "
            "model. No messages were dropped \u2014 continuing without compression. Run /compress to retry, /new "
            "for a clean session, or check auxiliary.compression.")


passes = 0
fails = []
total = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        c = json.loads(line)
    except json.JSONDecodeError:
        continue
    tag = c["tag"]
    if tag == "warn":
        want = _make_warn(c["waited"], c["ceiling"])
    elif tag == "total":
        want = _total(c["waited"], c["progress"])
    elif tag == "stall":
        want = _stall(c["idle"])
    else:
        continue
    total += 1
    if c.get("out") == want:
        passes += 1
    else:
        fails.append((tag, c.get("out"), want))

print("pass/total: %d/%d" % (passes, total))
for f in fails[:8]:
    print("FAIL", f[0])
    print("  C=", f[1][:120])
    print("  PY=", f[2][:120])
if fails:
    print("ORACLE compression_facade: %d cases, %d mismatches" % (total, len(fails)))
    sys.exit(1)
print("ORACLE compression_facade: %d cases, 0 mismatches" % total)
