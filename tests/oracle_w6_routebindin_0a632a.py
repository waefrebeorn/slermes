#!/usr/bin/env python3
"""Differential oracle for agent/route_binding.py (w6_routebindin_0a632a gate).

route_binding imports `utils` (which pulls ruamel) at module scope and imports
the rest of its collaborators inside the function. We stub the parent packages
and every collaborator with deterministic seams; the C port mirrors the same
wrapper control flow. `agent` / `hermes_cli` parent packages are bootstrapped
empty so their heavy __init__ never runs.
"""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)

def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items(): setattr(m, k, v)
    sys.modules[name] = m
    return m

# parent-package bootstrap (empty __path__ prevents real __init__ execution)
_pkg_agent = _mod("agent"); _pkg_agent.__path__ = []
_pkg_cli = _mod("hermes_cli"); _pkg_cli.__path__ = []

def base_url_host_matches(url, host):
    rest = url.split("://", 1)[-1]
    h = rest.split("/")[0].split("?")[0].split("#")[0].split(":")[0].lower()
    return h == host.lower()

_mod("utils", base_url_host_matches=base_url_host_matches)

class PoolChoice:
    def __init__(self, model): self.model = model; self.pool = model

def resolve_client(provider, *, model, raw_codex, explicit_base_url,
                   explicit_api_key, api_mode, pool_choice):
    if provider == "no_client":
        return None, model
    s = f"{provider}|{explicit_base_url}|{explicit_api_key}|{api_mode}|{raw_codex}"
    return s, model

_mod("agent.auxiliary_client", resolve_provider_client=resolve_client)
_mod("agent.chat_completion_helpers",
     _fallback_api_mode_hint=lambda entry, provider, base: (
         bool(entry.get("api_mode_explicit", False)),
         entry.get("api_mode", "chat_completions")))
_mod("hermes_cli.fallback_config", resolve_entry_api_key=lambda entry: entry.get("api_key"))
_mod("hermes_cli.runtime_provider", PoolChoice=PoolChoice)
_mod("agent.secret_scope", get_secret=lambda name: "ollama-secret")
_mod("hermes_cli.model_normalize", normalize_model_for_provider=lambda model, provider: "norm:" + model)

SRC = PYROOT + "/agent/route_binding.py"
spec = importlib.util.spec_from_file_location("live_route_binding", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["live_route_binding"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") != "resolve":
        continue
    try:
        route = live.resolve_route_entry_client(
            r.get("entry", {}), r.get("provider", ""), r.get("model", ""),
            raw_codex=bool(r.get("raw_codex", True)))
        py = {
            "client": route.client,
            "model": route.model,
            "api_mode_explicit": bool(route.api_mode_explicit),
            "api_mode": route.api_mode,
            "pool_choice": route.pool_choice is not None,
        }
    except Exception as exc:
        print(f"LIVE ERROR args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_routebindin_0a632a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
