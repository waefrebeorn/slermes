#!/usr/bin/env python3
"""Differential oracle for hermes_cli/config_providers.py (gate w15_configprov_34dd3f).

Stubs the module-load route_identity import; all ported functions are pure.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    parent, _, child = name.rpartition(".")
    if parent and parent in sys.modules:
        setattr(sys.modules[parent], child, m)
    return m


hcli = _mod("hermes_cli")
hcli.__path__ = [Q + "/hermes_cli"]
_mod("hermes_cli.route_identity", normalize_route_base_url=lambda u: u)

SRC = Q + "/hermes_cli/config_providers.py"
spec = importlib.util.spec_from_file_location("hermes_cli.config_providers", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hermes_cli.config_providers"] = live
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
    try:
        if func == "canonical_api_mode":
            py = live._canonical_api_mode(r.get("api_mode", ""))
        elif func == "coerce_provider_id":
            py = live.coerce_provider_id(r.get("value"))
        elif func == "stringify_provider_map":
            py = live.stringify_provider_map(r.get("providers"))
        elif func == "find_provider_entry":
            py = list(live.find_provider_entry(r.get("providers"), r.get("key")))
        elif func == "normalize_provider_models":
            py = list(live._normalize_provider_models(r.get("models")))
        elif func == "coerce_ssl_verify":
            py = live._coerce_ssl_verify(r.get("value"))
        elif func == "normalize_extra_headers":
            py = live.normalize_extra_headers(r.get("extra_headers"))
        elif func == "is_provider_enabled":
            py = live.is_provider_enabled(r.get("provider_cfg"))
        elif func == "route_model_cfg":
            py = live._route_model_cfg(r.get("entry"), r.get("model", ""))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w15_configprov_34dd3f: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
