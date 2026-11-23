#!/usr/bin/env python3
"""Differential oracle for upstream Hermes pm/package.py (gate w5_package_78a049).

The declarative helpers are pure data. pm.filesystem (the sole import) is
loaded from the live quarry so compose_env's native() spelling is real.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m

pm = _mod("pm")
pm.__path__ = []
fs_spec = importlib.util.spec_from_file_location("pm.filesystem", Q + "/pm/filesystem.py")
fs = importlib.util.module_from_spec(fs_spec)
sys.modules["pm.filesystem"] = fs
fs_spec.loader.exec_module(fs)

SRC = Q + "/pm/package.py"
spec = importlib.util.spec_from_file_location("w5_package_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w5_package_live"] = live
spec.loader.exec_module(live)

def package_from(args):
    p = live.Package()
    p.name = args.get("name", "")
    p.url = args.get("url", "")
    p.optional = args.get("optional", False)
    p.on_demand = tuple(args.get("on_demand") or ())
    p.gaps = {pair["target"]: pair["reason"] for pair in (args.get("gaps") or [])}
    return p

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
    got = r.get("out")
    args = {k: v for k, v in r.items() if k not in ("func", "out")}
    try:
        if func == "compose":
            py = live.compose_env(args.get("diffs") or [], args.get("base") or {})
        elif func == "error":
            py = str(live.InstallError(args.get("package", ""), args.get("cause", ""),
                                       args.get("remedy")))
        elif func == "entry":
            py = package_from(args).store_entry(args.get("version", ""), args.get("target", ""))
        elif func == "url":
            py = package_from(args).fetch_url(args.get("version", ""), args.get("target", ""))
        elif func == "optional":
            py = package_from(args).is_optional(args.get("target", ""))
        elif func == "reason":
            py = package_from(args).missing_reason(args.get("target", ""))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR func={func} args={args}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH func={func} args={args} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w5_package_78a049: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
