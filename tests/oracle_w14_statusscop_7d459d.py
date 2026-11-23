#!/usr/bin/env python3
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)
try:
    import ruamel.yaml  # noqa: F401
except Exception:
    _ru = types.ModuleType("ruamel"); _ry = types.ModuleType("ruamel.yaml")
    class _YAMLError(Exception): pass
    class _VersionedResolver: pass
    class _YAML:
        def __init__(self, *a, **k): pass
    _ry.YAML = _YAML; _ry.YAMLError = _YAMLError
    _re = types.ModuleType("ruamel.yaml.error"); _re.YAMLError = _YAMLError
    _rs = types.ModuleType("ruamel.yaml.resolver"); _rs.VersionedResolver = _VersionedResolver
    for _k, _v in (("ruamel", _ru), ("ruamel.yaml", _ry),
                   ("ruamel.yaml.error", _re), ("ruamel.yaml.resolver", _rs)):
        sys.modules[_k] = _v

"""Differential oracle for gateway/status_scoped_locks.py:_same_owner."""
SRC = QUARRY + "/gateway/status_scoped_locks.py"
spec = importlib.util.spec_from_file_location("w14_statusscop_7d459d_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

def compute(r):
    if r.get("func") != "same_owner":
        return None
    return bool(live._same_owner(r.get("existing") or {}, r.get("holder") or {}))

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w14_statusscop_7d459d: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
