#!/usr/bin/env python3
"""Differential oracle for w17_sessioning_11caf7 (gateway/session_ingress_burst.py).

AST-extracts the LIVE holds_busy_burst / _serial / _kind (the module cannot be
imported standalone: it pulls the whole gateway runtime) into a registered
module namespace, and drives them with scripted adapter/event objects.
"""
import ast, builtins, json, sys, types, __future__
try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
SRC = QUARRY + "/gateway/session_ingress_burst.py"
NAME = "w17_sessioning_11caf7"
ANN = __future__.annotations.compiler_flag

TREE = ast.parse(open(SRC, encoding="utf-8").read())
MOD = types.ModuleType("w17_live_burst"); sys.modules["w17_live_burst"] = MOD
NS = MOD.__dict__
NS["__builtins__"] = builtins
# seam: event.message_type from the fixture is this same sentinel (Python str equality).
NS["MessageType"] = types.SimpleNamespace(PHOTO="photo")
WANTED = {"holds_busy_burst", "_serial", "_kind"}
for node in TREE.body:
    if isinstance(node, ast.FunctionDef) and node.name in WANTED:
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                     flags=ANN, dont_inherit=True), NS)
for name in WANTED:
    if name not in NS:
        raise SystemExit("ORACLE %s: missing live %s" % (NAME, name))


def fake_adapter(r):
    d = dict(r.get("adapter") or {})
    flag = bool(d.get("_is_queue_text_debounce_candidate", False))
    d["_is_queue_text_debounce_candidate"] = lambda ev: flag
    return types.SimpleNamespace(**d)


def compute(r):
    f = r.get("func")
    if f == "holds_busy_burst":
        return bool(NS["holds_busy_burst"](fake_adapter(r), r.get("session_key", "")))
    if f == "serial":
        return bool(NS["_serial"](fake_adapter(r)))
    if f == "kind":
        evd = r.get("event") or {}
        ev = types.SimpleNamespace(message_type=evd.get("message_type", ""),
                                   internal=bool(evd.get("internal", False)))
        return NS["_kind"](fake_adapter(r), ev)
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except BaseException as ex:
        py = {"__raise__": type(ex).__name__}
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE %s: %d cases, %d mismatches" % (NAME, cases, mism))
sys.exit(1 if (mism or cases == 0) else 0)
