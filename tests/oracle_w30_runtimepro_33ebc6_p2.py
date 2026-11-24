#!/usr/bin/env python3
"""Differential oracle for hermes_cli/runtime_provider_custom.py (gate w30_runtimepro_33ebc6_p2).

Identity recovery (bare ``custom`` -> ``custom:<name>``) and the runtime
derivation decisions.  The live module is imported BY FILE PATH; every
collaborator that is not a pure function of the values under test is installed
as a seam carrying the answer the fixture declares:

  * the config file, the credential store, the named-entry lookup, the frozen
    route config, the credential pool, the host-gated env key candidates, the
    owning ``providers:`` entry, the managed local endpoint (llama.cpp),
    ``hermes_cli.models``' OpenCode helpers, ``resolve_provider_full`` and the
    per-runtime constructor ``_runtime``.

What is compared is then exactly what those values decide.
"""
import importlib.util
import json
import os
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w30_runtimepro_33ebc6_p2"

sys.path.append(str(HERE))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()
if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import hermes_cli.runtime_provider as rp_origin  # noqa: E402
import hermes_cli.models  # noqa: E402,F401  (imported so its helpers can be stubbed below)
import hermes_cli.providers  # noqa: E402,F401
from hermes_cli.auth import has_usable_secret  # noqa: E402
from agent.custom_entry_pool import entry_declares_credential  # noqa: E402

_spec = importlib.util.spec_from_file_location(
    "rp_custom_live", QUARRY / "hermes_cli" / "runtime_provider_custom.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)


class AuthError(Exception):
    pass


class LocalRuntimeUnavailableError(ValueError):
    def __init__(self, message, *, retryable=False):
        super().__init__(message)
        self.retryable = retryable


class _NoFrozen:
    def get(self):
        return None


WANTED = {
    "has_usable_secret", "entry_declares_credential", "model_id_matches",
    "entry_serves_model", "find_custom_identity", "find_custom_provider_identity",
    "find_custom_provider_identity_by_model", "routed_custom_identity",
    "empty_provider_identity", "endpoint_identity", "session_provider_identity",
    "canonical_custom_identity", "is_routable_provider",
    "custom_provider_request_overrides", "apply_custom_provider_extras",
    "expand_direct_api_alias", "opencode_family_for_custom", "entry_credential",
    "owning_entry_credential", "owned_config_url_credential",
    "resolve_direct_alias_runtime", "resolve_named_custom_runtime",
}

SEAM_KEYS = {
    "secrets", "canonical", "auth_error", "registered", "compatible", "normalized",
    "named_entry", "model_cfg", "resolved", "family", "pool", "host_gated",
    "declared", "entry_key", "config_base_url", "managed_endpoint", "detect_mode",
    "opencode_api_mode", "opencode_base_url", "load_config", "entry",
    "key_cmd_buildable", "target_model",
}


def seams_of(req):
    seams = dict(req.get("seams") or {})
    for key in SEAM_KEYS:
        if key in req and key not in seams:
            seams[key] = req[key]
    return seams


def _module(name, package=False):
    mod = sys.modules.get(name)
    if mod is None:
        mod = types.ModuleType(name)
        sys.modules[name] = mod
    if package:
        mod.__path__ = getattr(mod, "__path__", [])
    return mod


class _TokenProvider:
    """Stand-in for the per-request command token provider."""

    def __init__(self, cmd):
        self.cmd = cmd

    def __repr__(self):
        return "<token provider %s>" % self.cmd


def _jsonable(value):
    """Project a live return value onto JSON-comparable data."""
    if isinstance(value, _TokenProvider):
        return {"__token_provider__": value.cmd}
    if isinstance(value, tuple):
        return [_jsonable(v) for v in value]
    return value


def wire(req):
    seams = seams_of(req)
    secrets = seams.get("secrets") or {}
    config = req.get("config")
    if config is None:
        config = seams.get("load_config") or {}

    live.get_secret_str = lambda name, default="": secrets.get(name, default)
    rp_origin.load_config = lambda: config
    rp_origin.get_compatible_custom_providers = lambda config=None: seams.get("compatible")
    rp_origin.AuthError = AuthError
    rp_origin.auth_mod = types.SimpleNamespace(
        resolve_provider=lambda name: seams.get("canonical"))
    rp_origin._get_model_config = lambda: seams.get("model_cfg") or {}
    rp_origin._host_gated_env_key_candidates = lambda base_url, ollama=False: list(seams.get("host_gated") or [])
    rp_origin._config_base_url_for_provider = lambda model_cfg, provider: seams.get("config_base_url") or ""
    rp_origin._try_resolve_from_custom_pool = lambda *a, **k: seams.get("pool")
    rp_origin._detect_api_mode_for_url = lambda base_url: seams.get("detect_mode")
    rp_origin._FROZEN_RUNTIME_CONFIG = _NoFrozen()
    rp_origin._resolves_to_custom = lambda name: bool(seams.get("resolves_to_custom"))
    rp_origin.has_usable_secret = has_usable_secret

    def _named(name, **kw):
        if "named_entry" in seams:
            return seams["named_entry"]
        return live._get_named_custom_provider(name)
    rp_origin._get_named_custom_provider = _named

    def _runtime(provider, api_mode, base_url, api_key, **kw):
        out = {"provider": provider, "api_mode": api_mode,
               "base_url": base_url, "api_key": api_key}
        out.update(kw)
        return out
    rp_origin._runtime = _runtime

    # the owning providers: entry, as custom_entry_for_endpoint would answer it
    def _owning(base_url, provider_name=None, config=None):
        entry = seams.get("entry")
        return (str(entry.get("name") or "custom"), entry) if isinstance(entry, dict) else None
    live.custom_entry_for_endpoint = _owning
    live.entry_declares_credential = entry_declares_credential
    live.entry_scoped_pool = lambda pool, pool_key, entry, base_url=None: pool

    # llama.cpp endpoint / managed server
    pkg = _module("hermes_cli.local_runtime", package=True)
    endpoint_mod = _module("hermes_cli.local_runtime.endpoint")
    endpoint_mod._state_endpoint = lambda: seams.get("managed_endpoint")
    endpoint_mod.resolve_llamacpp_endpoint = lambda config=None: seams.get("managed_endpoint")
    setattr(pkg, "endpoint", endpoint_mod)

    # hermes_cli.models' OpenCode helpers
    models_mod = _module("hermes_cli.models")
    models_mod.opencode_provider_family = lambda name: seams.get("family")
    models_mod.opencode_model_api_mode = lambda family, model: seams.get("opencode_api_mode")
    models_mod.normalize_opencode_base_url = lambda family, api_mode, base_url: (
        seams.get("opencode_base_url") or base_url)

    # hermes_cli.providers.resolve_provider_full
    providers_mod = _module("hermes_cli.providers")

    def _resolve_full(name, providers, custom):
        answer = seams.get("resolved")
        return None if answer is None else (object() if answer else None)
    providers_mod.resolve_provider_full = _resolve_full

    # the command token provider (short-lived credential minting)
    cmd_src = _module("agent.command_token_source")
    cmd_src.build_command_token_provider = (
        lambda cmd, name: _TokenProvider(cmd) if seams.get("key_cmd_buildable") else None)

    return config, seams


def _path(path):
    return list(path) if path is not None else None


def f_has_usable_secret(req, config, seams):
    return has_usable_secret(req.get("value"))


def f_entry_declares_credential(req, config, seams):
    return entry_declares_credential(req.get("entry"))


def f_model_id_matches(req, config, seams):
    return live._model_id_matches(req.get("value"), str(req.get("target") or "").strip().lower())


def f_entry_serves_model(req, config, seams):
    return live._entry_serves_model(req.get("entry") or {}, str(req.get("model") or "").strip().lower())


def _matches_for(mode, target):
    if mode == "url":
        return lambda entry: live._route_key(live._entry_url(entry)) == target
    return lambda entry: live._entry_serves_model(entry, target)


def f_find_custom_identity(req, config, seams):
    mode = req.get("mode")
    raw = req.get("target")
    if mode == "url":
        target = live._route_key(raw)
    else:
        target = str(raw or "").strip().lower()
    prefer = req.get("prefer")
    found = live._find_custom_identity(_matches_for(mode, target), config,
                                       prefer if isinstance(prefer, str) and prefer else None)
    return found


def f_find_custom_provider_identity(req, config, seams):
    return live.find_custom_provider_identity(req.get("base_url"))


def f_find_custom_provider_identity_by_model(req, config, seams):
    return live.find_custom_provider_identity_by_model(req.get("model"))


def f_routed_custom_identity(req, config, seams):
    return live.routed_custom_identity(req.get("base_url"), req.get("model"),
                                       config, req.get("held"))


def f_empty_provider_identity(req, config, seams):
    return live.empty_provider_identity(req.get("base_url"), req.get("model"))


def f_endpoint_identity(req, config, seams):
    return live._endpoint_identity(req.get("base_url"))


def f_session_provider_identity(req, config, seams):
    return live.session_provider_identity(
        req.get("provider"), req.get("requested_provider"), req.get("base_url"),
        req.get("model"), config=config)


def f_canonical_custom_identity(req, config, seams):
    env = seams.get("env") or {}
    for key in list(os.environ):
        if key == "HERMES_INFERENCE_PROVIDER":
            del os.environ[key]
    for key, value in env.items():
        os.environ[str(key)] = str(value)
    return live.canonical_custom_identity(
        base_url=req.get("base_url"),
        config_provider=req.get("config_provider"),
        model=req.get("model"),
        requested=req.get("requested"))


def f_is_routable_provider(req, config, seams):
    return live.is_routable_provider(req.get("provider"))


def f_custom_provider_request_overrides(req, config, seams):
    return live._custom_provider_request_overrides(req.get("custom_provider") or {})


def f_apply_custom_provider_extras(req, config, seams):
    result = dict(req.get("result") or {})
    live._apply_custom_provider_extras(req.get("custom_provider") or {}, req.get("target_model"), result)
    return result


def f_expand_direct_api_alias(req, config, seams):
    return _jsonable(live.expand_direct_api_alias(req.get("provider"), req.get("existing_base")))


def f_opencode_family_for_custom(req, config, seams):
    return live._opencode_family_for_custom(req.get("requested_provider") or "",
                                            req.get("base_url") or "")


def f_entry_credential(req, config, seams):
    return _jsonable(live._entry_credential(rp_origin, req.get("entry") or {},
                                            str((req.get("entry") or {}).get("name") or "custom")))


def f_owning_entry_credential(req, config, seams):
    declared, key = live._owning_entry_credential(rp_origin, req.get("base_url") or "")
    return {"declared": bool(declared), "key": _jsonable(key)}


def f_owned_config_url_credential(req, config, seams):
    declared, key = live.owned_config_url_credential(
        req.get("base_url") or "", req.get("model_cfg") or {}, req.get("provider") or "")
    return {"declared": bool(declared), "key": _jsonable(key)}


def f_resolve_direct_alias_runtime(req, config, seams):
    return live._resolve_direct_alias_runtime(
        req.get("requested_provider") or "", req.get("explicit_api_key"),
        req.get("explicit_base_url") or "")


def f_resolve_named_custom_runtime(req, config, seams):
    try:
        return live._resolve_named_custom_runtime(
            requested_provider=req.get("requested_provider") or "",
            explicit_api_key=req.get("explicit_api_key"),
            explicit_base_url=req.get("explicit_base_url"),
            target_model=req.get("target_model"),
            config=config)
    except live.LocalRuntimeUnavailableError as exc:
        return {"error": "local_runtime_unavailable", "retryable": bool(exc.retryable)}


DISPATCH = {
    "has_usable_secret": f_has_usable_secret,
    "entry_declares_credential": f_entry_declares_credential,
    "model_id_matches": f_model_id_matches,
    "entry_serves_model": f_entry_serves_model,
    "find_custom_identity": f_find_custom_identity,
    "find_custom_provider_identity": f_find_custom_provider_identity,
    "find_custom_provider_identity_by_model": f_find_custom_provider_identity_by_model,
    "routed_custom_identity": f_routed_custom_identity,
    "empty_provider_identity": f_empty_provider_identity,
    "endpoint_identity": f_endpoint_identity,
    "session_provider_identity": f_session_provider_identity,
    "canonical_custom_identity": f_canonical_custom_identity,
    "is_routable_provider": f_is_routable_provider,
    "custom_provider_request_overrides": f_custom_provider_request_overrides,
    "apply_custom_provider_extras": f_apply_custom_provider_extras,
    "expand_direct_api_alias": f_expand_direct_api_alias,
    "opencode_family_for_custom": f_opencode_family_for_custom,
    "entry_credential": f_entry_credential,
    "owning_entry_credential": f_owning_entry_credential,
    "owned_config_url_credential": f_owned_config_url_credential,
    "resolve_direct_alias_runtime": f_resolve_direct_alias_runtime,
    "resolve_named_custom_runtime": f_resolve_named_custom_runtime,
}


def expected(req):
    func = req.get("func")
    fn = DISPATCH.get(func)
    if fn is None:
        raise AssertionError("unknown func %r" % (func,))
    config, seams = wire(req)
    return fn(req, config, seams)


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except ValueError:
            continue
        if not isinstance(req, dict) or req.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(req)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (req.get("func"), req.get("case"), exc), file=sys.stderr)
            continue
        got = req.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (req.get("func"), req.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if (mism or cases == 0) else 0)


if __name__ == "__main__":
    main()
