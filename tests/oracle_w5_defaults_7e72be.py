#!/usr/bin/env python3
"""Oracle for upstream Hermes pm/defaults.py (gate w5_defaults_7e72be).

pm/defaults.py imports registry/store/environment lazily inside its functions;
stub pm.* modules are registered in sys.modules (parent-package bootstrap) and
the registry metadata is injected per case from the fixture.
"""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)

_pkg = types.ModuleType("pm"); _pkg.__path__ = [Q + "/pm"]
sys.modules["pm"] = _pkg
for m in ("pm.environments", "pm.paths", "pm.package", "pm.registry", "pm.store", "pm.filesystem"):
    sys.modules[m] = types.ModuleType(m)
sys.modules["pm.package"].InstallError = type("InstallError", (Exception,), {})

spec = importlib.util.spec_from_file_location("live_defaults_w5", Q + "/pm/defaults.py")
live = importlib.util.module_from_spec(spec)
sys.modules["live_defaults_w5"] = live
spec.loader.exec_module(live)

class Pkg:
    def __init__(self, d):
        self.default = bool(d.get("default", False))
        self.internal = bool(d.get("internal", False))
        self.missing = bool(d.get("missing", False))
    def missing_reason(self, target):
        return "missing" if self.missing else None

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
        if func == "canonical":
            py = live.canonical_default_name(r.get("name", ""))
        elif func == "declined":
            data = json.loads(r.get("text", ""))
            names = data.get("declined") if isinstance(data, dict) else None
            py = sorted({live.canonical_default_name(x) for x in (names or ())
                         if isinstance(x, str)})
        elif func == "defaults":
            reg = {p["name"]: Pkg(p) for p in r.get("pkgs", [])
                   if p.get("found", True)}
            regmod = sys.modules["pm.registry"]
            regmod.get_package = lambda n, reg=reg: reg[n]
            storem = sys.modules["pm.store"]
            storem.current_target = lambda: "linux"
            py = live.default_packages(list(r.get("names", [])),
                                       declined_names=set(r.get("declined", [])))
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
print(f"ORACLE w5_defaults_7e72be: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
