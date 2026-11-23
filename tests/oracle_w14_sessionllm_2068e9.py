#!/usr/bin/env python3
"""Differential oracle for gateway/session_llm_oneshot.py:_validate."""
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

SRC = QUARRY + "/gateway/session_llm_oneshot.py"
spec = importlib.util.spec_from_file_location("w14_sessionllm_2068e9_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

def _patch_templates(templates):
    pkg = sys.modules.get("agent")
    if pkg is None:
        pkg = types.ModuleType("agent"); pkg.__path__ = []; sys.modules["agent"] = pkg
    fake = types.ModuleType("agent.oneshot")
    fake.PROMPT_TEMPLATES = {t: (lambda v=None: ("", "")) for t in templates}
    sys.modules["agent.oneshot"] = fake
    pkg.oneshot = fake

def compute(r):
    if r.get("func") != "validate":
        return None
    _patch_templates(r.get("templates") or [])
    try:
        live._validate(r.get("params") or {})
        return {"ok": True}
    except Exception as exc:
        return {"ok": False, "reason": getattr(exc, "reason", None),
                "field": getattr(exc, "field", None), "detail": getattr(exc, "detail", None)}

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
print("ORACLE w14_sessionllm_2068e9: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
