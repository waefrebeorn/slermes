#!/usr/bin/env python3
"""Oracle for upstream Hermes hermes_cli/main_query_file.py.

stdin, file IO, stderr and sys.exit are stubbed from the fixture; the gate
compares args.query mutation, the exit code and the exact error lines.
"""
import importlib.util, json, sys, io, types, builtins
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
SRC = Q + "/hermes_cli/main_query_file.py"
spec = importlib.util.spec_from_file_location("qfile_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    if func == "is_empty":
        py = not (r.get("value") or "").strip()
        cases += 1
        got = r.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH is_empty args={r} py={py!r} c={got!r}", file=sys.stderr)
        continue
    if func != "read": continue
    qf_val = r.get("query_file")
    args = types.SimpleNamespace(query_file=qf_val, query=r.get("query"))
    buf = io.StringIO()
    code = [None]
    def _exit(c): raise SystemExit(c)
    live.sys = types.SimpleNamespace(
        stdin=types.SimpleNamespace(read=lambda: r.get("stdin_content", "")),
        stderr=buf, exit=_exit)
    def fake_open(path, *a, **kw):
        if not r.get("read_ok", True):
            raise OSError(r.get("err_detail", ""))
        c = r.get("file_content", "")
        if c.startswith("\ufeff"): c = c[1:]  # utf-8-sig
        return io.StringIO(c)
    real_open = builtins.open
    builtins.open = fake_open
    try:
        live._read_query_file(args)
    except SystemExit as e:
        code[0] = e.code
    finally:
        builtins.open = real_open
    noop = not qf_val
    action = "noop" if noop else ("exit" if code[0] is not None else "set")
    py = {"action": action, "code": code[0], "query": args.query,
          "stderr": buf.getvalue() or None}
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH read args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE qfile: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
