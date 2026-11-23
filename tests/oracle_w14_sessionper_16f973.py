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

"""Differential oracle for gateway/session_persistence.py module helpers."""
SRC = QUARRY + "/gateway/session_persistence.py"
spec = importlib.util.spec_from_file_location("w14_sessionper_16f973_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

_EXC = {"RuntimeError": RuntimeError, "ValueError": ValueError,
        "Exception": Exception, "TypeError": TypeError, "OSError": OSError}

def compute(r):
    func = r.get("func")
    if func == "chat_labels_meta_key":
        return live.SessionPersistenceMixin._chat_labels_meta_key(
            r.get("platform"), r.get("scope_id"), r.get("chat_id"))
    if func == "is_live_system_guard":
        cls = _EXC.get(r.get("exc_type"), Exception)
        exc = cls(r.get("exc_message", ""))
        return bool(live._is_live_system_guard(exc))
    return None

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
print("ORACLE w14_sessionper_16f973: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
