#!/usr/bin/env python3
"""Oracle for upstream Hermes agent/lazy_forward.py."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
SRC = "C:/Users/eman5/hermes-agent/agent/lazy_forward.py"
spec = importlib.util.spec_from_file_location("lazyfwd_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)
cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    if func not in ("forward", "forward_static", "doc"): continue
    module = r.get("module", "")
    name = r.get("name", "")
    if func == "doc":
        py = live.forward(module, name).__doc__
    else:
        static = (func == "forward_static") or bool(r.get("static", False))
        fn = live.forward(module, name, static=static)
        is_static = isinstance(fn, staticmethod)
        g = fn.__func__ if is_static else fn
        py = {"name": g.__name__, "qualname": g.__qualname__,
              "doc": g.__doc__, "static": is_static}
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE lazyfwd: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
