#!/usr/bin/env python3
"""Differential oracle for hermes_cli/prune_advisor.py (gate w10_pruneadvis_d472b3).

Pure sizing/formatting/advice helpers only; disk/DB side effects are out of scope.
"""
import importlib.util, json, re, sys, types

_INTRE = re.compile(r"^-?[0-9]+$")
def _coerce(v):
    if isinstance(v, str) and _INTRE.fullmatch(v.strip()):
        return int(v)
    if isinstance(v, dict):
        return {k: _coerce(x) for k, x in v.items()}
    if isinstance(v, list):
        return [_coerce(x) for x in v]
    return v

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
PKG = "w10_pruneadvis_live_pkg"
_pkg = types.ModuleType(PKG); _pkg.__path__ = [Q + "/hermes_cli"]; sys.modules[PKG] = _pkg
spec = importlib.util.spec_from_file_location(PKG + ".prune_advisor", Q + "/hermes_cli/prune_advisor.py")
live = importlib.util.module_from_spec(spec); sys.modules[spec.name] = live; spec.loader.exec_module(live)


def _snap(d):
    return live.DiskSnapshot(d["db_bytes"], d["free_bytes"], d["total_bytes"])


def _advice(a):
    d = a["disk"]
    return live.Advice(_snap(d), a["retention_days"], a["prunable_sessions"],
                       a["total_sessions"], a["reclaim_bytes"],
                       a.get("estimate_ok", True), a.get("vacuum_after_prune", True))


def compute(r):
    r = _coerce(r)
    func = r.get("func")
    if func == "low":
        return _snap({"db_bytes": r.get("db_bytes", 0), "free_bytes": r.get("free_bytes", 0),
                      "total_bytes": r.get("total_bytes", 0)}).low
    if func == "retention_days":
        return live._retention_days(r.get("sessions", {}))
    if func == "gb":
        return live._gb(int(r.get("n", 0)))
    if func == "sessions":
        return live._sessions(int(r.get("n", 0)))
    if func == "helps":
        return _advice({"disk": {"db_bytes": r.get("db_bytes", 0), "free_bytes": r.get("free_bytes", 0),
                                  "total_bytes": r.get("total_bytes", 0)},
                        "retention_days": 90, "prunable_sessions": r.get("prunable_sessions", 0),
                        "total_sessions": 0, "reclaim_bytes": r.get("reclaim_bytes", 0)}).helps
    if func == "advice_lines":
        return live.advice_lines(_advice(r.get("advice", {})), command_prefix=r.get("command_prefix", "/"))
    if func == "notice_payload":
        return live.notice_payload(_advice(r.get("advice", {})))
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)
print("ORACLE w10_pruneadvis_d472b3: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
