#!/usr/bin/env python3
"""Differential oracle for hermes_cli/auth_zai_kimi.py (gate w10_authzaiki_f897c0)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/auth_zai_kimi.py"
sys.path.insert(0, QUARRY)

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg

# auth_zai_kimi imports httpx from auth_constants at module load; the pure URL
# helpers never use it, so an inert stub is sufficient (no network).
_ac = types.ModuleType("hermes_cli.auth_constants")
_ac.httpx = types.SimpleNamespace(post=lambda *a, **k: None)
sys.modules["hermes_cli.auth_constants"] = _ac
sys.modules["hermes_cli"].auth_constants = _ac

spec = importlib.util.spec_from_file_location("w10_authzaiki_f897c0_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w10_authzaiki_f897c0_live"] = live
spec.loader.exec_module(live)


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    func = r.get("func")
    if func == "kimi":
        return live._resolve_kimi_base_url(r.get("api_key"), r.get("default_url"),
                                           r.get("env_override") or "")
    if func == "lm":
        return live._normalize_lmstudio_runtime_base_url(r.get("base_url"))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except json.JSONDecodeError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w10_authzaiki_f897c0: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH w10_authzaiki_f897c0 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE w10_authzaiki_f897c0: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
