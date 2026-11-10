#!/usr/bin/env python3
"""Oracle for hermes_cli/auth_commands.py _provider_base_url + _is_known_provider
(port include/port_auth_providers.c). The live fns read module globals
(PROVIDER_REGISTRY, OPENROUTER_BASE_URL, CUSTOM_POOL_PREFIX,
_configured_provider_entry, inline _get_custom_provider_config); the C port
takes the registry/config/custom base URLs as params. We ast-extract the live
fns and seed those globals from the harness's reg/cfg/cust/in_registry fields so
the oracle still runs the LIVE code, not a copy:
  burl:  _provider_base_url(provider) with seeded globals
  iknow: _is_known_provider(provider, configured_provider) seeded by configured flag
"""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def _listify(v):
    return v if isinstance(v, list) else json.loads(v or "[]")


class _P:
    def __init__(self, base): self.inference_base_url = base


def main() -> int:
    src = open("C:/Users/eman5/hermes-agent/hermes_cli/auth_commands.py",
               encoding="utf-8").read()
    g = {"Any": None}
    got = set()
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name in ("_provider_base_url",
                                                         "_is_known_provider",
                                                         "_configured_provider_entry",
                                                         "_resolve_custom_provider_input"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), "ap", "exec"), g)
            got.add(n.name)
        elif isinstance(n, ast.Assign):
            for tar in n.targets:
                if isinstance(tar, ast.Name) and tar.id in ("OPENROUTER_BASE_URL",
                                                            "CUSTOM_POOL_PREFIX",
                                                            "_PROVIDER_ALIASES"):
                    exec(compile(ast.Module(body=[n], type_ignores=[]), "ap", "exec"), g)
    if not {"_provider_base_url", "_is_known_provider"} <= got:
        print("ORACLE auth_providers: live fns not all extracted", sorted(got))
        return 1
    bu = g["_provider_base_url"]
    ikn = g["_is_known_provider"]
    rsl = g.get("_resolve_custom_provider_input")

    total, mism = 0, 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        import json
        c = json.loads(line)
        total += 1
        fam = c.get("family")
        provider = c.get("provider") or ""
        if fam == "burl":
            reg = c.get("reg") or ""
            cfg = c.get("cfg") or ""
            cust = c.get("cust") or ""
            # seed live globals from this case's harness fields
            registry = {}
            if reg:
                registry[provider] = _P(reg)
            empty_entry = dict()
            def conf_entry(p, _cfg=cfg):
                return {"base_url": _cfg} if _cfg else None
            spec = {"OPENROUTER_BASE_URL": "https://openrouter.ai/api/v1",
                    "CUSTOM_POOL_PREFIX": "custom:",
                    "PROVIDER_REGISTRY": registry,
                    "_configured_provider_entry": conf_entry}
            # custom pool path inline-imports agent.credential_pool._get_custom_provider_config
            import sys as _sys
            from types import ModuleType
            cp = ModuleType("agent.credential_pool")
            cp._get_custom_provider_config = lambda prov: {"base_url": cust} if cust else {}
            par = ModuleType("agent"); par.__path__ = []
            _sys.modules.setdefault("agent", par)
            _sys.modules["agent.credential_pool"] = cp
            bu.__globals__.update(spec)
            want = bu(provider)
            if want != c.get("out"):
                mism += 1
                print(f"MISMATCH burl case{c.get('case')} prov={provider!r} C={c.get('out')!r} PY={want!r}")
        elif fam == "iknow":
            in_reg, configured = int(c.get("inreg", 0)), int(c.get("cfg", 0))
            import sys as _sys
            from types import ModuleType
            cp = ModuleType("agent.credential_pool")
            cp._get_custom_provider_config = lambda prov: {}
            par = ModuleType("agent"); par.__path__ = []
            _sys.modules.setdefault("agent", par)
            _sys.modules["agent.credential_pool"] = cp
            spec = {"OPENROUTER_BASE_URL": "https://openrouter.ai/api/v1",
                    "CUSTOM_POOL_PREFIX": "custom:",
                    "PROVIDER_REGISTRY": {provider: object()} if in_reg else {},
                    "_configured_provider_entry": lambda p: {} if configured else None}
            ikn.__globals__.update(spec)
            want = bool(ikn(provider, {} if configured else None))
            if want != bool(c.get("out")):
                mism += 1
                print(f"MISMATCH iknow case{c.get('case')} prov={provider!r} C={c.get('out')} PY={want}")
        elif fam == "resolve":
            raw = c.get("raw") or ""
            entries = _listify(c.get("entries"))
            spec = {"CUSTOM_POOL_PREFIX": "custom:",
                    "_get_custom_provider_entries": lambda: entries,
                    "_normalize_custom_pool_name": lambda name: str(name or "").strip().lower().replace(" ", "-")}
            rsl.__globals__.update(spec)
            want = rsl(raw)
            want = want if want is not None else ""
            if want != (c.get("out") or ""):
                mism += 1
                print(f"MISMATCH resolve case{c.get('case')} raw={raw!r} C={c.get('out')!r} PY={want!r}")
    if total == 0:
        print("ORACLE auth_providers: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE auth_providers: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
