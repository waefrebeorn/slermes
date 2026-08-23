"""twin for auth_is_provider_explicitly_configured.

Imports the LIVE hermes_cli/auth.py::is_provider_explicitly_configured and
traces the SAME rung ladder as the C harness:
  B0 empty / R1 active_provider / R2a model.provider /
  R2b moa refs (+per-slot) / R2c aggregator / R2d presets (+per-slot) /
  R3 api_key env vars / R4 credential-pool sources.

Per input the twin materializes the encoded state for real:
  - HERMES_HOME -> fresh tmpdir with auth.json (active_provider +
    credential_pool) and config.yaml (cfg_json)
  - OPENAI_API_KEY set/cleared per the envspec
so LIVE reads real files/env exactly as production does.

Input encoding: "<provider|active|cfg_json|envspec|pool_json>"
('-' = absent).
"""
import json as _json
import os
import sys
import tempfile
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

spec = ilu.spec_from_file_location(
    "wc_live_auth", os.path.join(DEV, "hermes_cli", "auth.py"))
auth = ilu.module_from_spec(spec)
sys.modules["wc_live_auth"] = auth
spec.loader.exec_module(auth)


def _norm(v):
    return (v or "").strip().lower()


def _slot_matches(slot, norm):
    return (
        isinstance(slot, dict)
        and _norm(slot.get("provider")) == norm
        and bool(_norm(slot.get("provider")))
    )


def trace(provider, active, cfg, envspec, pool):
    c = Cup("is_provider_explicitly_configured")
    norm = _norm(provider)
    if not norm:                                        # B0
        c.branch(True)
        c.valstr("false")
        return c.finalize()
    c.branch(False)

    # 1. auth.json active_provider
    act = _norm(active)
    if act and act == norm:                             # R1
        c.branch(True)
        c.valstr("true")
        return c.finalize()
    c.branch(False)

    # 2. config.yaml model.provider + MoA slots
    if cfg:
        model = cfg.get("model")
        if isinstance(model, dict):
            cp = _norm(model.get("provider"))
            if cp and cp == norm:                       # R2a
                c.branch(True)
                c.valstr("true")
                return c.finalize()
        c.branch(False)
        moa = cfg.get("moa")
        if isinstance(moa, dict):
            for slot in moa.get("reference_models") or []:
                if _slot_matches(slot, norm):           # R2b
                    c.branch(True)
                    c.valstr("true")
                    return c.finalize()
                c.branch(False)
            if _slot_matches(moa.get("aggregator"), norm):   # R2c
                c.branch(True)
                c.valstr("true")
                return c.finalize()
            c.branch(False)
            presets = moa.get("presets")
            if isinstance(presets, dict):
                for preset in presets.values():
                    if not isinstance(preset, dict):
                        continue
                    for slot in preset.get("reference_models") or []:
                        if _slot_matches(slot, norm):   # R2d
                            c.branch(True)
                            c.valstr("true")
                            return c.finalize()
                        c.branch(False)
                    if _slot_matches(preset.get("aggregator"), norm):
                        c.branch(True)
                        c.valstr("true")
                        return c.finalize()
                    c.branch(False)

    # 3. provider-specific env vars (excluding CLAUDE_CODE_OAUTH_TOKEN)
    pconfig = auth.PROVIDER_REGISTRY.get(norm)
    implicit = {"CLAUDE_CODE_OAUTH_TOKEN"}
    if pconfig and pconfig.auth_type == "api_key":      # R3 gate
        c.branch(True)
        for env_var in pconfig.api_key_env_vars:
            if env_var in implicit:
                continue
            if auth.has_usable_secret(os.getenv(env_var, "")):
                c.branch(True)
                c.valstr("true")
                return c.finalize()
            c.branch(False)
    else:
        c.branch(False)

    # 4. explicit-flow credential-pool entries
    entries = pool if isinstance(pool, list) else []
    for entry in entries:
        if not isinstance(entry, dict):
            continue
        source = _norm(entry.get("source"))
        if not source:
            continue
        if source.startswith("env:"):
            c.branch(True)
            env_var = entry.get("source", "").split(":", 1)[1].strip()
            if env_var and auth.has_usable_secret(os.getenv(env_var, "")):
                c.branch(True)                          # R4 env-backed
                c.valstr("true")
                return c.finalize()
            c.branch(False)
            continue
        c.branch(False)
        if (
            source in {"device_code", "loopback_pkce", "hermes_pkce", "manual"}
            or source.startswith("manual:")
        ):
            c.branch(True)                              # R4 explicit flow
            c.valstr("true")
            return c.finalize()
        c.branch(False)

    c.valstr("false")
    return c.finalize()


def run(inputs):
    import yaml
    cups = []
    old_home = os.environ.get("HERMES_HOME")
    try:
        for raw in inputs:
            provider, rest = raw.split("|", 1)
            active_s, rest = rest.split("|", 1)
            cfg_s, rest = rest.split("|", 1)
            envspec, pool_s = rest.split("|", 1)

            home = tempfile.mkdtemp(prefix="wcup_auth_")
            os.environ["HERMES_HOME"] = home

            store = {}
            pool = []
            if pool_s != "-":
                pool = _json.loads(pool_s)
            if pool or active_s != "-":
                store["providers"] = {}
                if active_s != "-":
                    store["active_provider"] = active_s
                if pool:
                    store["credential_pool"] = {provider.strip().lower(): pool}
                with open(os.path.join(home, "auth.json"), "w") as f:
                    _json.dump(store, f)

            cfg = None
            if cfg_s != "-":
                cfg = _json.loads(cfg_s)
                with open(os.path.join(home, "config.yaml"), "w") as f:
                    yaml.safe_dump(cfg, f)

            if envspec == "-":
                os.environ.pop("OPENAI_API_KEY", None)
            else:
                var, _, val = envspec.partition("=")
                os.environ[var] = val

            cups.append(trace(provider, active_s if active_s != "-" else "",
                              cfg, envspec,
                              pool))
    finally:
        if old_home is not None:
            os.environ["HERMES_HOME"] = old_home
        else:
            os.environ.pop("HERMES_HOME", None)
    return cups
