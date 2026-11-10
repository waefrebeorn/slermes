#!/usr/bin/env python3
"""Oracle for gateway/parked_route_refusal.py _notice_due. The function is
ast-extracted and run live on a stub adapter; the monotonic clock is stubbed.
The sent entries table is shared with the C side (emitted once, both re-key).
"""
import ast
import json
import sys
from types import SimpleNamespace

SRC = "C:/Users/eman5/hermes-agent/gateway/parked_route_refusal.py"
code = open(SRC, encoding="utf-8").read()
tree = ast.parse(code)

seg = None
for n in tree.body:
    if isinstance(n, ast.FunctionDef) and n.name == "_notice_due":
        seg = ast.get_source_segment(code, n)

NS = {"_monotonic": lambda: 0.0, "PARKED_NOTICE_WINDOW_S": 60.0}
exec(compile(ast.parse(seg), "x", "exec"), NS)
fn = NS["_notice_due"]


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
        print("ORACLE parked_notice: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        NS["_monotonic"] = lambda n0=c["now"]: n0
        # rebuild adapter._parked_notice_sent_at as tuple-keyed dict
        sent = {}
        for e in (c["sent"] or []):
            sent[(e["profile"], e["platform"], e["chat"])] = e["ts"]
        adapter = SimpleNamespace(_parked_notice_sent_at=sent)
        due = fn(adapter, c["p"], c["pl"], c["ch"])
        # expected out
        want_sent = sorted(
            ({"profile": k[0], "platform": k[1], "chat": k[2], "ts": v}
             for k, v in adapter._parked_notice_sent_at.items()),
            key=lambda _: "0")
        want_sent.sort(key=lambda e: (e["profile"], e["platform"], e["chat"]))
        got_sent = sorted(
            (e for e in c["out"]["sent"]),
            key=lambda e: (e["profile"], e["platform"], e["chat"]))
        if c["out"]["due"] != due or got_sent != want_sent:
            mism += 1
            print(f"MISMATCH due {c['case']}: C_due={c['out']['due']} PY_due={due} "
                  f"C_sent={c['out']['sent']} PY_sent={want_sent}")
    print(f"ORACLE parked_notice: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
