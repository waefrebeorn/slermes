#!/usr/bin/env python3
"""Oracle for persistence/core/canonical.py (canonical_dumps, canonical_json).
The live module is imported via importlib with a pre-registered persistence.core
package so its relative ``from .errors import AuthorityRefusal`` resolves.
"""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = r"C:\Users\eman5\hermes-agent"

def pre():
    e = types.ModuleType("persistence.core.errors")
    class AuthorityRefusal(Exception): pass
    e.AuthorityRefusal = AuthorityRefusal
    sys.modules["persistence.core.errors"] = e
    p = types.ModuleType("persistence"); p.__path__ = []
    pc = types.ModuleType("persistence.core"); pc.__path__ = []
    sys.modules["persistence"] = p; sys.modules["persistence.core"] = pc

spec = importlib.util.spec_from_file_location(
    "persistence.core.canonical", QUARRY + r"\persistence\core\canonical.py")
live = importlib.util.module_from_spec(spec)
live.__package__ = "persistence.core"
sys.modules["persistence.core.canonical"] = live
pre()
spec.loader.exec_module(live)

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
    value = r.get("value")
    try:
        py = live.canonical_dumps(value) if func == "dumps" else live.canonical_json(value)
    except Exception as e:
        py = f"__EXC__:{type(e).__name__}:{e}"
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH canon case={r.get('case')} func={func} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE canon: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
