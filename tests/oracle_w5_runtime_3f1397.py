#!/usr/bin/env python3
"""Oracle for upstream Hermes pm/runtime.py (gate w5_runtime_3f1397).

pm/runtime.py imports pm.filesystem/pm.package at load; those are stubbed in
sys.modules (parent-package bootstrap). subprocess.run is injected per case so
the LIVE _validate expression is exercised, and _resident_runtime/runtime_python
are monkeypatched so the LIVE runtime_command is exercised for both contracts.
"""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)

_pkg = types.ModuleType("pm"); _pkg.__path__ = [Q + "/pm"]
sys.modules["pm"] = _pkg
_fs = types.ModuleType("pm.filesystem"); _fs.native = lambda p: p
sys.modules["pm.filesystem"] = _fs
_pk = types.ModuleType("pm.package")
_pk.InstallError = type("InstallError", (Exception,), {})
sys.modules["pm.package"] = _pk

spec = importlib.util.spec_from_file_location("live_runtime_w5", Q + "/pm/runtime.py")
live = importlib.util.module_from_spec(spec)
sys.modules["live_runtime_w5"] = live
spec.loader.exec_module(live)

class S:
    def __init__(self, s): self.s = s
    def __str__(self): return self.s

def set_resident(python, site, resident):
    live._resident_runtime = (lambda: (S(python), S(site))) if resident else (lambda: None)
    live.runtime_python = lambda *, bootstrap, cache: S(python)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "validate":
            rc = r.get("rc", 0)
            stderr = r.get("stderr", "")
            live.subprocess.run = (lambda *a, se=stderr, rc=rc, **kw:
                                   types.SimpleNamespace(stderr=se, returncode=rc))
            py = live._validate("python", {})
        elif func == "command":
            set_resident(r.get("python", ""), r.get("site", ""),
                         bool(r.get("resident", False)))
            py = live.runtime_command(S(r.get("script", "")),
                                      tuple(r.get("args", [])))
        elif func == "launcher":
            set_resident(r.get("python", ""), r.get("site", ""), True)
            py = live.runtime_command(S(r.get("script", "")), ())[5]
        else:
            continue
    except Exception as exc:
        print(f"MISMATCH {func} {r}: python raised {exc!r}", file=sys.stderr)
        mism += 1
        cases += 1
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w5_runtime_3f1397: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
