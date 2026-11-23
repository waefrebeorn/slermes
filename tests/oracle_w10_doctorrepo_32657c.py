#!/usr/bin/env python3
"""Differential oracle for gate w10_doctorrepo_32657c (hermes_cli/doctor_report.py)."""
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
NAME = "w10_doctorrepo_32657c_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "hermes_cli", "doctor_report.py"))
live = importlib.util.module_from_spec(sp); sys.modules[NAME] = live; sp.loader.exec_module(live)

# Capture the doctor output primitive that each helper dispatches to.
calls = []
def _spy(kind):
    def f(*a): calls.append((kind, a))
    return f
live.check_ok = _spy("ok"); live.check_warn = _spy("warn"); live.check_fail = _spy("fail")

def mkfinding(d):
    return live.Finding(issues=list((d or {}).get("issues", [])),
                        manual_issues=list((d or {}).get("manual_issues", [])),
                        fixed=int((d or {}).get("fixed", 0)))

class FakePath:
    def __init__(self, exists): self._exists = exists
    def exists(self): return self._exists
    def mkdir(self, *a, **k): return None

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func"); calls.clear()
    try:
        if func == "check_bool":
            ret = live.check_bool(r.get("cond", False), r.get("ok", "ok"), r.get("bad", "bad"),
                                  fail=bool(r.get("fail", False)))
            kind, args = calls[0]
            py = {"ret": ret, "kind": kind, "text": args[0],
                  "detail": args[1] if len(args) > 1 else None}
        elif func == "finding_merge":
            a = mkfinding(r.get("first")); b = mkfinding(r.get("second"))
            a.merge(b)
            py = {"issues": a.issues, "manual_issues": a.manual_issues, "fixed": a.fixed}
        elif func == "ensure_dir":
            f = live.Finding()
            live.ensure_dir(f, bool(r.get("should_fix", False)), FakePath(bool(r.get("exists", False))),
                            r.get("exists_msg", ""), r.get("created_msg", ""), r.get("missing_msg", ""))
            kind, args = calls[0]
            py = {"kind": kind, "msg": args[0], "detail": args[1] if len(args) > 1 else None, "fixed": f.fixed}
        else: continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr); continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w10_doctorrepo_32657c: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
