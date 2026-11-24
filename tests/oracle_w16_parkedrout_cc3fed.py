#!/usr/bin/env python3
"""Differential oracle for gate w16_parkedrout_cc3fed (gateway/parked_route_refusal.py)."""
import importlib.util, json, logging, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
logging.disable(logging.CRITICAL)
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)
def _mod(name):
    m = types.ModuleType(name); m.__path__ = [os.path.join(Q, name)]; sys.modules[name] = m; return m
_mod("gateway")
# Bootstrap the parent packages + the single base class the live module imports at module scope.
_mod("persistence"); _mod("persistence.core")
errs = _mod("persistence.core.errors")
class AuthorityRefusal(Exception):
    def __init__(self, reason, *, field=None, detail=None):
        super().__init__(reason); self.reason = reason; self.field = field; self.detail = detail
errs.AuthorityRefusal = AuthorityRefusal
NAME = "w16_parkedrout_cc3fed_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "gateway", "parked_route_refusal.py"))
live = importlib.util.module_from_spec(sp); sys.modules[NAME] = live; sp.loader.exec_module(live)
NS = types.SimpleNamespace

def canon(v):
    if isinstance(v, (list, tuple)): return [canon(x) for x in v]
    if isinstance(v, dict): return {k: canon(x) for k, x in v.items()}
    return v

def sent_entries(sent):
    return sorted([[p,pf,ch,t] for (p,pf,ch),t in sent.items()], key=lambda x:(x[0],x[1],x[2]))

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    try:
        if func == "refused_info":
            e = live.ParkedRouteRefused()
            py = {"code": e.code, "status": e.status,
                  "headers": {k:v for k,v in e.headers.items()}, "message": str(e)}
        elif func == "profile_refusal":
            e = live.ParkedProfileRefusal(r.get("profile"))
            py = {"reason": e.reason, "field": e.field, "detail": e.detail, "profile": e.profile}
        elif func == "constants":
            py = {"notice": live.PARKED_ROUTE_NOTICE, "window": live.PARKED_NOTICE_WINDOW_S}
        elif func == "notice_due":
            sent = {(e[0], e[1], e[2]): e[3] for e in (r.get("sent") or [])}
            adapter = NS(_parked_notice_sent_at=sent)
            live._monotonic = lambda: r.get("now", 0.0)
            due = live._notice_due(adapter, r.get("profile"), r.get("platform"), r.get("chat_id"))
            py = {"due": due, "sent": sent_entries(adapter._parked_notice_sent_at)}
        else: continue
        py = canon(py)
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr); continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w16_parkedrout_cc3fed: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
