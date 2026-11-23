#!/usr/bin/env python3
"""Differential oracle for upstream Hermes pm/install.py (gate w5_install_184801).

Only pure helpers are exercised. The module's many pm imports are stubbed so
it loads; _dependents_first's walk() is replaced with a fixture-driven one.
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

def _stub(name, *symbols, **attrs):
    m = _mod(name, **attrs)
    for s in symbols:
        setattr(m, s, type(s, (), {}))
    return m

pm = _mod("pm")
pm.__path__ = []
_mod("pm.paths")
_stub("pm.downloader", "DownloadPaused", "ProgressFn")
_mod("pm.filesystem", native=lambda x: x, remove_tree=lambda *a: None, retry_held=lambda f: f())
_stub("pm.lock", "Facts", "Lockfile")
_stub("pm.package", "InstallError", "Package", "Runner", "StatePackage", compose_env=lambda *a, **k: {})
_stub("pm.plugin_inputs", "Candidates", "Members", "PluginInput", "Selection", "StagedUpdate")
_mod("pm.registry", get_package=lambda n: None, walk=lambda names: [])
_mod("pm.store", Store=object, current_target=lambda: "", digest_matches=lambda *a: True,
     entry_component=lambda s: s, fetch_entry_name=lambda a: a, merge_tree=lambda *a: None,
     tree_digest=lambda p: "")

SRC = Q + "/pm/install.py"
spec = importlib.util.spec_from_file_location("w5_install_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w5_install_live"] = live
spec.loader.exec_module(live)

class _FakeLock:
    def __init__(self, arts):
        self.arts = arts
    def artifacts(self, name, target):
        return [{"sha256": s} for s in self.arts]

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
        if func == "extra":
            py = live._extra_key(args.get("name", ""))
        elif func == "identity":
            py = live._identity(_FakeLock(args.get("artifacts") or []), "x", args.get("target", ""))
            if py is not None:
                py = [py[0], list(py[1])]
        elif func == "order":
            names = args.get("names") or []
            deps = args.get("deps") or {}
            live.walk = lambda ns: [types.SimpleNamespace(name=d) for d in deps.get(ns[0], [])]
            py = live._dependents_first(list(names))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR func={func} args={args}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH func={func} args={args} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w5_install_184801: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
