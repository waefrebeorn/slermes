#!/usr/bin/env python3
"""Differential oracle for hermes_cli/runtime_provider_custom.py (gate w30_runtimepro_33ebc6_p1).

The live module is imported BY FILE PATH (its package context pulls the whole
CLI import graph on this host).  Every collaborator that is not a pure function
of the values under test is installed as a seam first, with the answer the
fixture declares:

  * ``live.get_secret_str`` / ``rp.load_config`` / ``rp.get_compatible_custom_providers``
    / ``rp._get_named_custom_provider`` / ``rp._get_model_config`` /
    ``rp.auth_mod.resolve_provider`` / ``rp._parse_api_mode``'s plugin registry,
  * ``hermes_cli.config_providers._normalize_custom_provider_entry`` (the
    compatible-entry view's per-entry normaliser).

What is compared is then purely what these values decide.
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
GATE = "w30_runtimepro_33ebc6_p1"

# Hermetic home: _key_env_secret consults the configured providers, so the gate
# must not read this developer's real config.yaml (it reported
# "custom_providers ... is a dict, not a list" and flipped one case between runs).
import tempfile as _tempfile

_TMPHOME = _tempfile.mkdtemp(prefix="w30rp_")
os.environ["HERMES_HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME
os.environ["HOME"] = _TMPHOME
(_p := Path(_TMPHOME) / "config.yaml").write_text("version: 54\n", encoding="utf-8")

sys.path.append(str(HERE))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()
if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import hermes_cli.runtime_provider as rp_origin  # noqa: E402
from hermes_cli import config_providers as cfgprov  # noqa: E402
from hermes_cli.providers import custom_provider_aliases, custom_provider_slug  # noqa: E402
from hermes_cli.route_identity import normalize_route_base_url  # noqa: E402
from utils import base_url_hostname  # noqa: E402

_spec = importlib.util.spec_from_file_location(
    "rp_custom_live", QUARRY / "hermes_cli" / "runtime_provider_custom.py")
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)


class AuthError(Exception):
    pass


WANTED = {
    "normalize_custom_provider_name", "normalize_base_url_for_match", "clean",
    "custom_provider_slug", "custom_provider_aliases", "normalize_route_base_url",
    "base_url_hostname", "route_key", "entry_url", "filter_capabilities",
    "is_provider_enabled", "normalize_extra_headers", "canonical_api_mode",
    "parse_api_mode", "shadowed_by_builtin", "builtin_named_entries",
    "builtin_name_user_entry", "persisted_provider_id", "key_env_secret",
    "model_cfg_key_env_for", "lift_model_capabilities", "lift_common_custom_fields",
    "match_new_style_provider", "match_legacy_custom_provider", "compatible_entry_path",
    "select_named_custom_provider", "named_custom_provider_config_path",
    "custom_provider_config_path_for_url", "codex_model_provider_id",
    "has_named_custom_provider",
}


def _transports():
    """The plugin transport registry, as one more seam."""
    mod = sys.modules.get("agent.transports")
    if mod is None:
        mod = types.ModuleType("agent.transports")
        sys.modules["agent.transports"] = mod
    return mod


def _norm_entry_factory(config, seams):
    """``_normalize_custom_provider_entry`` answered from the fixture's parallel
    ``normalized`` array (index-aligned with ``custom_providers``)."""
    legacy = config.get("custom_providers") if isinstance(config, dict) else None
    legacy = legacy if isinstance(legacy, list) else []
    normalized = seams.get("normalized") or []

    def _norm(entry, **_kw):
        for i, raw in enumerate(legacy):
            if raw is entry:
                return normalized[i] if i < len(normalized) else None
        for i, raw in enumerate(legacy):
            if raw == entry:
                return normalized[i] if i < len(normalized) else None
        return None
    return _norm


def _resolver(seams):
    def resolve(_name):
        if seams.get("auth_error"):
            raise AuthError("unresolvable")
        return seams.get("canonical")
    return resolve


SEAM_KEYS = {
    "secrets", "canonical", "auth_error", "registered", "compatible", "normalized",
    "named_entry", "model_cfg", "resolved", "family", "pool", "host_gated",
    "declared", "entry_key", "config_base_url", "managed_endpoint", "detect_mode",
    "opencode_api_mode", "opencode_base_url", "load_config",
}


def seams_of(req):
    """Seam answers, accepted either nested under "seams" or at the top level."""
    seams = dict(req.get("seams") or {})
    for key in SEAM_KEYS:
        if key in req and key not in seams:
            seams[key] = req[key]
    return seams


def wire(req):
    """Install every external answer this case declares; return (config, seams)."""
    seams = seams_of(req)
    secrets = seams.get("secrets") or {}
    config = req.get("config")
    if config is None:
        config = seams.get("load_config") or {}

    live.get_secret_str = lambda name, default="": secrets.get(name, default)
    rp_origin.load_config = lambda: config
    rp_origin.get_compatible_custom_providers = lambda config=None: seams.get("compatible")
    rp_origin.AuthError = AuthError
    rp_origin.auth_mod = types.SimpleNamespace(resolve_provider=_resolver(seams))
    rp_origin._get_model_config = lambda: seams.get("model_cfg") or {}

    def _named(name, **_kw):
        if "named_entry" in seams:
            return seams["named_entry"]
        return live._get_named_custom_provider(name)
    rp_origin._get_named_custom_provider = _named

    _transports().registered_api_modes = lambda: list(seams.get("registered") or [])
    cfgprov._normalize_custom_provider_entry = _norm_entry_factory(config, seams)
    return config, seams


def _path(path):
    return list(path) if path is not None else None


def f_normalize_custom_provider_name(req, config, seams):
    return live._normalize_custom_provider_name(req.get("value") or "")


def f_normalize_base_url_for_match(req, config, seams):
    return live._normalize_base_url_for_match(req.get("value"))


def f_clean(req, config, seams):
    return live._clean(req.get("value"))


def f_custom_provider_slug(req, config, seams):
    return custom_provider_slug(req.get("display_name") or "", req.get("provider_key") or "")


def f_custom_provider_aliases(req, config, seams):
    return sorted(custom_provider_aliases(req.get("display_name") or "", req.get("provider_key") or ""))


def f_normalize_route_base_url(req, config, seams):
    return normalize_route_base_url(req.get("value"))


def f_base_url_hostname(req, config, seams):
    return base_url_hostname(req.get("value") or "")


def f_route_key(req, config, seams):
    return live._route_key(req.get("value"))


def f_entry_url(req, config, seams):
    return live._entry_url(req.get("entry") or {})


def f_filter_capabilities(req, config, seams):
    return live._filter_capabilities(req.get("value"))


def f_is_provider_enabled(req, config, seams):
    return cfgprov.is_provider_enabled(req.get("entry"))


def f_normalize_extra_headers(req, config, seams):
    return cfgprov.normalize_extra_headers(req.get("value"))


def f_canonical_api_mode(req, config, seams):
    return cfgprov._canonical_api_mode(req.get("value") or "")


def f_parse_api_mode(req, config, seams):
    return rp_origin._parse_api_mode(req.get("value"))


def f_shadowed_by_builtin(req, config, seams):
    return live._shadowed_by_builtin(req.get("requested_norm") or "")


def f_builtin_named_entries(req, config, seams):
    return live.builtin_named_entries(req.get("requested_norm") or "", req.get("providers") or {})


def f_builtin_name_user_entry(req, config, seams):
    return live.builtin_name_user_entry(req.get("name") or "", req.get("providers"))


def f_persisted_provider_id(req, config, seams):
    return live.persisted_provider_id(req.get("provider") or "", req.get("providers"))


def f_key_env_secret(req, config, seams):
    return live._key_env_secret(req.get("entry") or {}, "case")


def f_model_cfg_key_env_for(req, config, seams):
    return live._model_cfg_key_env_for(req.get("model_cfg") or {}, req.get("base_url") or "")


def f_lift_model_capabilities(req, config, seams):
    out = {}
    live._lift_model_capabilities(req.get("entry") or {}, req.get("model"), out)
    return out.get("capabilities")


def f_lift_common_custom_fields(req, config, seams):
    out = dict(req.get("result") or {})
    live._lift_common_custom_fields(
        req.get("entry") or {}, out,
        provider_key=req.get("provider_key") or "",
        key_env=req.get("key_env") or "",
        api_mode=req.get("api_mode"))
    return out


def f_match_new_style_provider(req, config, seams):
    found = live._match_new_style_provider(
        req.get("requested_norm") or "",
        req.get("providers") or {},
        resolve_credentials=req.get("resolve_credentials", True))
    return {"entry": found[0], "key": found[1]} if found else None


def f_match_legacy_custom_provider(req, config, seams):
    found = live._match_legacy_custom_provider(
        req.get("requested_norm") or "", req.get("custom_providers") or [])
    return {"entry": found[0], "raw": found[1]} if found else None


def f_compatible_entry_path(req, config, seams):
    return _path(live._compatible_entry_path(req.get("config") or {}, req.get("entry") or {}))


def f_select_named_custom_provider(req, config, seams):
    found = live._select_named_custom_provider(
        req.get("requested_provider") or "", config=config,
        resolve_credentials=req.get("resolve_credentials", True))
    return {"entry": found[0], "path": _path(found[1])} if found else None


def f_named_custom_provider_config_path(req, config, seams):
    return _path(live.named_custom_provider_config_path(
        req.get("requested_provider") or "", config))


def f_custom_provider_config_path_for_url(req, config, seams):
    return _path(live.custom_provider_config_path_for_url(req.get("base_url"), config))


def f_codex_model_provider_id(req, config, seams):
    return live.codex_model_provider_id(req.get("requested_provider") or "")


def f_has_named_custom_provider(req, config, seams):
    return live.has_named_custom_provider(req.get("requested_provider") or "")


DISPATCH = {
    "normalize_custom_provider_name": f_normalize_custom_provider_name,
    "normalize_base_url_for_match": f_normalize_base_url_for_match,
    "clean": f_clean,
    "custom_provider_slug": f_custom_provider_slug,
    "custom_provider_aliases": f_custom_provider_aliases,
    "normalize_route_base_url": f_normalize_route_base_url,
    "base_url_hostname": f_base_url_hostname,
    "route_key": f_route_key,
    "entry_url": f_entry_url,
    "filter_capabilities": f_filter_capabilities,
    "is_provider_enabled": f_is_provider_enabled,
    "normalize_extra_headers": f_normalize_extra_headers,
    "canonical_api_mode": f_canonical_api_mode,
    "parse_api_mode": f_parse_api_mode,
    "shadowed_by_builtin": f_shadowed_by_builtin,
    "builtin_named_entries": f_builtin_named_entries,
    "builtin_name_user_entry": f_builtin_name_user_entry,
    "persisted_provider_id": f_persisted_provider_id,
    "key_env_secret": f_key_env_secret,
    "model_cfg_key_env_for": f_model_cfg_key_env_for,
    "lift_model_capabilities": f_lift_model_capabilities,
    "lift_common_custom_fields": f_lift_common_custom_fields,
    "match_new_style_provider": f_match_new_style_provider,
    "match_legacy_custom_provider": f_match_legacy_custom_provider,
    "compatible_entry_path": f_compatible_entry_path,
    "select_named_custom_provider": f_select_named_custom_provider,
    "named_custom_provider_config_path": f_named_custom_provider_config_path,
    "custom_provider_config_path_for_url": f_custom_provider_config_path_for_url,
    "codex_model_provider_id": f_codex_model_provider_id,
    "has_named_custom_provider": f_has_named_custom_provider,
}


def expected(req):
    func = req.get("func")
    fn = DISPATCH.get(func)
    if fn is None:
        raise AssertionError("unknown func %r" % (func,))
    config, seams = wire(req)
    return fn(req, config, seams)


def main():
    cases = mism = unknown = 0
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
