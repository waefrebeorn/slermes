#!/usr/bin/env python3
"""Differential oracle for gate w10_gatewaytoo_83997c (hermes_cli/gateway_tools.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)
def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items(): setattr(m, k, v)
    sys.modules[name] = m
    return m
h = _mod("hermes_cli"); h.__path__ = [os.path.join(Q, "hermes_cli")]
NAME = "w10_gatewaytoo_83997c_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "hermes_cli", "gateway_tools.py"))
live = importlib.util.module_from_spec(sp); sys.modules[NAME] = live; sp.loader.exec_module(live)
NS = types.SimpleNamespace

def make_view(rows=None, text=None):
    v = live.GatewayProcessView.__new__(live.GatewayProcessView)
    v.agent = NS(session_id="s")
    v._tail = None
    v.rows = [live._Proc(r) for r in (rows or [])]
    class Conn:
        def call(self, *a, **k): return {"available": True, "text": text}
    v.connection = Conn()
    return v

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    try:
        if func == "proc_from_row":
            p = live._Proc(r.get("row") or {})
            py = {"id": p.id, "command": p.command, "exit_code": p.exit_code,
                  "started_at": p.started_at, "exited_at": p.exited_at,
                  "output_buffer": p.output_buffer, "completion_reason": p.completion_reason,
                  "exited": p.exited}
        elif func == "filter_printable":
            py = make_view(text=r.get("text", "")).subagent_tail("sa", fetch=True)
        elif func == "interesting":
            py = make_view(rows=r.get("rows"))._interesting(float(r.get("now", 0)))
        else: continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr); continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w10_gatewaytoo_83997c: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
