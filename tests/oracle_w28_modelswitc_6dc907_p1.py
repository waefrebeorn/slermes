#!/usr/bin/env python3
"""Differential oracle for hermes_cli/model_switch_providers.py (gate w28_modelswitc_6dc907_p1).

The live module is imported by package path (``hermes_cli.model_switch_providers``)
and only its pure value functions are called.  Every I/O seam they reach for --
``auth.json``, the credential pool, models.dev / PROVIDER_REGISTRY, ``anon_auth``
guest state, botocore, ``build_command_token_provider`` -- is replaced by the
value the fixture says it would return.  Nothing is mocked that the C port
implements: the comparison is over the decisions themselves.
"""
import contextlib
import json
import os
import sys
import types
from pathlib import Path
from unittest import mock

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_modelswitc_6dc907_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

import hermes_cli.model_switch_providers as live  # noqa: E402
import hermes_cli.anon_auth as anon  # noqa: E402
import hermes_cli.models as models_mod  # noqa: E402

WANTED = {
    "catalog_stale", "any_env", "skip", "azure_entra_configured", "fast_aws_signal",
    "aws_creds_for_listing", "is_aws_sdk", "first_curated", "free_tier_nous_row",
    "cap_models", "split_chain_entries", "extend_unique", "norm_url",
    "entry_base_url", "entry_api_mode", "entry_credentials", "discover_flag",
    "display_prefix", "group_display_name",
}


@contextlib.contextmanager
def env_of(mapping):
    """Run with os.environ replaced by the fixture mapping."""
    mapping = mapping if isinstance(mapping, dict) else {}
    saved = dict(os.environ)
    try:
        os.environ.clear()
        for k, v in mapping.items():
            if isinstance(v, str):
                os.environ[str(k)] = v
        yield
    finally:
        os.environ.clear()
        os.environ.update(saved)


@contextlib.contextmanager
def anon_state(flags):
    flags = flags if isinstance(flags, dict) else {}
    with mock.patch.object(anon, "has_guest", lambda: bool(flags.get("guest"))), \
         mock.patch.object(anon, "guest_allowed", lambda: bool(flags.get("guest_allowed"))), \
         mock.patch.object(anon, "free_inference_active", lambda state=None: bool(flags.get("free_inference"))), \
         mock.patch.object(anon, "FREE_TIER_LABEL", flags.get("label", "Nous \u00b7 free tier")), \
         mock.patch.object(anon, "GUEST_MODEL", flags.get("guest_model", "nous/welcome")):
        yield


def _aws_creds(flag):
    return mock.patch("agent.bedrock_adapter.has_aws_credentials", lambda: bool(flag))


def azure(req):
    cfg = req.get("model_cfg")
    cfg = cfg if isinstance(cfg, dict) else {}
    env = req.get("env") or {}
    with mock.patch.object(models_mod, "_get_model_config_dict", lambda: dict(cfg)):
        return live._azure_entra_configured(lambda name: env.get(name))


def credentials(req):
    entry = req.get("entry") or {}
    keys = tuple(req.get("key_env_keys") or ("key_env", "api_key_env"))
    identity = req.get("key_cmd_identity")

    class _Source:
        def __init__(self, cache_identity):
            self.cache_identity = cache_identity

    factory = (lambda cmd, name: _Source(identity)) if identity is not None else (lambda cmd, name: None)
    with mock.patch.object(live, "build_command_token_provider", factory):
        _src, key_env, ident = live._entry_credentials(entry, *keys)
    return {"key_env": key_env, "identity": ident, "source": identity is not None}


def split_chain(req):
    models = list(req.get("models") or [])
    live._split_chain_entries(models)
    return models


def extend_unique(req):
    target = list(req.get("target") or [])
    live._extend_unique(target, req.get("items") or [])
    return target


def compute(r):
    f = r.get("func")
    if f == "catalog_stale":
        return live._discovered_catalog_stale(r.get("entry") or {}, r.get("model_ids"))
    if f == "any_env":
        env = r.get("env") or {}
        return live._any_env(r.get("env_vars") or [], lambda name: env.get(name))
    if f == "skip":
        return live._skip(set(r.get("seen") or []), set(r.get("excluded") or []),
                          *(r.get("keys") or []))
    if f == "azure_entra_configured":
        return azure(r)
    if f == "fast_aws_signal":
        with env_of(r.get("env")):
            return live._has_fast_aws_sdk_signal()
    if f == "aws_creds_for_listing":
        with env_of(r.get("env")), _aws_creds(r.get("sdk_creds")):
            return live._has_aws_sdk_creds_for_listing(r.get("slug") or "",
                                                       r.get("current_provider") or "")
    if f == "is_aws_sdk":
        raw = r.get("pconfig")
        pconfig = types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw
        return live._is_aws_sdk(pconfig)
    if f == "first_curated":
        return live._first_curated(r.get("curated") or {}, r.get("keys") or [])
    if f == "free_tier_nous_row":
        with anon_state(r.get("flags")):
            return live._free_tier_nous_row(r.get("row") or {})
    if f == "cap_models":
        return live._cap_models(list(r.get("model_ids") or []), r.get("max_models"),
                                r.get("slug") or "")
    if f == "split_chain_entries":
        return split_chain(r)
    if f == "extend_unique":
        return extend_unique(r)
    if f == "norm_url":
        return live._norm_url(r.get("url"))
    if f == "entry_base_url":
        entry = r.get("entry") or {}
        keys = r.get("keys")
        if isinstance(keys, list):
            return live._entry_base_url(entry, tuple(keys))
        return live._entry_base_url(entry)
    if f == "entry_api_mode":
        return live._entry_api_mode(r.get("entry") or {})
    if f == "entry_credentials":
        return credentials(r)
    if f == "discover_flag":
        return live._discover_flag(r.get("entry") or {})
    if f == "display_prefix":
        return live._display_prefix(r.get("name") or "")
    if f == "group_display_name":
        return live._group_display_name(r.get("name") or "")
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = compute(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
