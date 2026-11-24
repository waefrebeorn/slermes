#!/usr/bin/env python3
"""Differential oracle for hermes_cli/profile_model_seed.py (w17_profilemod_d242f9).

Imports the LIVE module with its three sibling modules stubbed:
  - _expand_env_vars is identity (fixtures carry no ${VAR} refs),
  - custom_provider_aliases = {name, provider_key} minus empties,
  - runtime provider lookups resolve nothing.
The pure requested/aliases/merge functions are then called directly on LIVE code.
"""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

pkg = types.ModuleType("hermes_cli")
pkg.__path__ = [os.path.join(Q, "hermes_cli")]
sys.modules.setdefault("hermes_cli", pkg)

cfg = types.ModuleType("hermes_cli.config")
cfg._expand_env_vars = lambda v: v
sys.modules["hermes_cli.config"] = cfg

prov = types.ModuleType("hermes_cli.providers")
prov.custom_provider_aliases = lambda name, key: frozenset(x for x in (name, key) if x)
sys.modules["hermes_cli.providers"] = prov

rpc = types.ModuleType("hermes_cli.runtime_provider_custom")
rpc._get_named_custom_provider = lambda provider, config=None, resolve_credentials=False: None
rpc._normalize_custom_provider_name = lambda p: p
rpc._shadowed_by_builtin = lambda p: False
rpc.named_custom_provider_config_path = lambda provider, view: None
sys.modules["hermes_cli.runtime_provider_custom"] = rpc

SRC = Q + "/hermes_cli/profile_model_seed.py"
spec = importlib.util.spec_from_file_location("hermes_cli.profile_model_seed", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hermes_cli.profile_model_seed"] = live
spec.loader.exec_module(live)


def compat(v):
    if isinstance(v, (set, frozenset)):
        return sorted((compat(x) for x in v), key=repr)
    if isinstance(v, (list, tuple)):
        return [compat(x) for x in v]
    if isinstance(v, dict):
        return {str(k): compat(x) for k, x in v.items()}
    if isinstance(v, int) and abs(v) > 9007199254740992:
        return str(v)
    return v


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
        if func == "requested":
            py = compat(live._requested_provider(r.get("model")))
        elif func == "aliases":
            py = compat(live._aliases(r.get("entry"), r.get("key", "")))
        elif func == "merge":
            py = compat(live._merge_endpoint_seed(
                r.get("dst"), r.get("seed"), frozenset(r.get("claimed") or [])))
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

print(f"ORACLE w17_profilemod_d242f9: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
