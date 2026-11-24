#!/usr/bin/env python3
"""Differential oracle for hermes_cli/runtime_provider_backends.py (gate
w23_runtimepro_54c1e6).

The module's own docstring fixes the seam: origin-internal collaborators are
resolved on ``hermes_cli.runtime_provider`` at call time via ``_rp()`` "so test
patches on hermes_cli.runtime_provider.* (...) still apply". The world a fixture
describes supplies exactly those answers -- the config read, api-mode parsing /
detection / inference, the bare-custom trust test, the credential-pool lookup,
the host-gated env key table, the provider registries and the Bedrock / Entra
adapter facts -- while the two credential-shape predicates stay REAL
(``hermes_cli.auth.has_usable_secret`` / ``looks_like_openrouter_key``), so the
port is compared against the implementation it mirrors.

Everything the module itself decides -- base_url precedence, the ``/v1`` strip,
auth_mode normalization, the Entra vs API-key branch, source labels, candidate
order, the alias->custom remap, the OpenRouter-context test, the pool
short-circuit, the placeholder key, and Bedrock's triple-path routing -- is the
live function.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "runtime_provider_backends.py"
GATE = "w23_runtimepro_54c1e6"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("runtime_provider_backends_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["runtime_provider_backends_live"] = live
_spec.loader.exec_module(live)

import agent.azure_identity_adapter as real_azure  # noqa: E402
import agent.bedrock_adapter as real_bedrock  # noqa: E402
import hermes_cli.config as real_config  # noqa: E402
import hermes_cli.runtime_provider as real_rp  # noqa: E402
import hermes_cli.runtime_provider_custom as real_custom  # noqa: E402
import providers as real_providers  # noqa: E402
from hermes_cli.auth import has_usable_secret, looks_like_openrouter_key  # noqa: E402

WANTED = {"azure_foundry", "openrouter", "bedrock", "is_external", "external_runtime"}


class AuthError(Exception):
    """The refusal type `hermes_cli.runtime_provider.AuthError` behaves as."""

    def __init__(self, message, code=None):
        super().__init__(message)
        self.code = code


class EntraIdentityConfig:
    def __init__(self, scope=""):
        self.scope = scope


class GuardedRegistry(dict):
    """`PROVIDER_REGISTRY.get` that raises when the fixture says the lookup failed."""

    def get(self, key, default=None):
        raise RuntimeError("registry unavailable")


class FakeRp(types.ModuleType):
    """`hermes_cli.runtime_provider` answering from the fixture's world."""

    def __init__(self, world):
        super().__init__("hermes_cli.runtime_provider")
        self.world = world
        self.AuthError = AuthError
        self.has_usable_secret = has_usable_secret
        self.looks_like_openrouter_key = looks_like_openrouter_key
        self._runtime = real_rp._runtime
        self.resolve_external_process_provider_credentials = (
            lambda provider: dict(world.get("creds") or {}))
        table = {
            str(k).strip().lower(): types.SimpleNamespace(
                auth_type=str(v.get("auth_type", "")),
                id=str(v.get("id", k)),
                inference_base_url=str(v.get("inference_base_url", "")),
            )
            for k, v in (world.get("registry") or {}).items()
        }
        self.PROVIDER_REGISTRY = GuardedRegistry(table) if world.get("registry_error") else table

    def _rp_seam(self, key, default=None):
        return (self.world.get("rp") or {}).get(key, default)

    def _cfg_provider(self, model_cfg):
        return self._rp_seam("cfg_provider", "")

    def _config_base_url_for_provider(self, model_cfg, provider):
        return self._rp_seam("cfg_base_url", "")

    def _parse_api_mode(self, raw):
        return self._rp_seam("parse_api_mode")

    def _azure_inferred_api_mode(self, effective_model, api_mode):
        return self._rp_seam("azure_inferred") or api_mode

    def _get_model_config(self):
        return self.world.get("model_cfg") or {}

    def _resolves_to_custom(self, name):
        return bool(self._rp_seam("resolves_to_custom", False))

    def _config_base_url_trustworthy_for_bare_custom(self, cfg_base_url, cfg_provider):
        return bool(self._rp_seam("trustworthy", False))

    def _detect_api_mode_for_url(self, base_url):
        return self._rp_seam("detect_api_mode")

    def _resolve_plain_custom_api_mode(self, model_cfg, base_url):
        return self._rp_seam("plain_custom_api_mode") or "chat_completions"

    def _try_resolve_from_custom_pool(self, base_url, provider_label, api_mode_override=None,
                                      provider_name=None):
        return self._rp_seam("pool_result")

    def _host_gated_env_key_candidates(self, base_url, *, ollama):
        return list(self._rp_seam("host_gated", []))


def module_with_getattr_raising(message):
    mod = types.ModuleType("agent.azure_identity_adapter")

    def __getattr__(name):  # noqa: D401 - module-level __getattr__ contract
        raise ImportError(message)

    mod.__getattr__ = __getattr__
    return mod


class Patcher:
    """Install the world's seams, then put every module back."""

    def __init__(self, world):
        self.world = world
        self.saved = []

    def _set(self, target, name, value):
        self.saved.append((target, name, getattr(target, name)))
        setattr(target, name, value)

    def __enter__(self):
        w = self.world
        aws = w.get("aws") or {}
        azure = w.get("azure") or {}
        secrets = w.get("secrets") or {}
        dotenv = w.get("dotenv") or {}
        self.fake_rp = FakeRp(w)

        self._set(live, "_rp", lambda: self.fake_rp)
        self._set(live, "get_secret_str", lambda name, default="": secrets.get(name, default))
        self._set(live, "is_token_provider",
                  lambda value: bool(w.get("explicit_is_token_provider")))
        self._set(live, "os", types.SimpleNamespace(
            environ={"AWS_BEARER_TOKEN_BEDROCK": str(aws.get("env_bearer", ""))}))

        self._set(real_config, "get_env_value", lambda name, *a, **k: dotenv.get(name, ""))
        self._set(real_config, "get_env_value_prefer_dotenv",
                  lambda name, *a, **k: dotenv.get(name, ""))
        self._set(real_config, "load_config", lambda *a, **k: {"bedrock": w.get("bedrock_cfg") or {}})

        owned = (w.get("rp") or {}).get("owned") or {}
        self._set(real_custom, "_model_cfg_key_env_for",
                  lambda model_cfg, base_url: (w.get("rp") or {}).get("key_env_for", ""))
        self._set(real_custom, "_owning_entry_credential",
                  lambda rp, base_url: (bool(owned.get("declared")), owned.get("key", "")))

        profiles = w.get("profiles") or {}

        def get_provider_profile(name):
            if w.get("profiles_error"):
                raise RuntimeError("profile registry unavailable")
            if name not in profiles:
                return None
            return types.SimpleNamespace(auth_type=str(profiles[name].get("auth_type", "")))

        self._set(real_providers, "get_provider_profile", get_provider_profile)

        self._set(real_bedrock, "has_aws_credentials", lambda *a, **k: bool(aws.get("has_credentials")))
        self._set(real_bedrock, "resolve_aws_auth_env_var",
                  lambda *a, **k: (aws.get("auth_env_var") or None))
        self._set(real_bedrock, "resolve_bedrock_runtime_region",
                  lambda *a, **k: str(aws.get("region", "us-east-1")))
        self._set(real_bedrock, "bedrock_guardrail_config",
                  lambda *a, **k: (aws.get("guardrail") or None))
        self._set(real_bedrock, "bedrock_openai_uses_mantle", lambda model: bool(aws.get("mantle")))
        self._set(real_bedrock, "is_anthropic_bedrock_model", lambda model: bool(aws.get("anthropic")))
        self._set(real_bedrock, "resolve_bedrock_bearer_token",
                  lambda *a, **k: str(aws.get("bearer", "")))

        mode = str(azure.get("adapter", "ok"))
        if mode == "fail":
            self.saved.append((sys.modules, "agent.azure_identity_adapter",
                               sys.modules.get("agent.azure_identity_adapter")))
            sys.modules["agent.azure_identity_adapter"] = module_with_getattr_raising(
                str(azure.get("fail_msg", "adapter unavailable")))
        else:
            self._set(real_azure, "SCOPE_AI_AZURE_DEFAULT", str(azure.get("default_scope", "")))
            self._set(real_azure, "EntraIdentityConfig", EntraIdentityConfig)

            def build_token_provider(config=None, **kwargs):
                if mode == "import_error":
                    raise ImportError(str(azure.get("import_error_msg", "")))
                return "token:%s" % (getattr(config, "scope", "") or "")

            self._set(real_azure, "build_token_provider", build_token_provider)
        return self

    def __exit__(self, *exc_info):
        for target, name, value in reversed(self.saved):
            if target is sys.modules:
                if value is None:
                    sys.modules.pop(name, None)
                else:
                    sys.modules[name] = value
            else:
                setattr(target, name, value)
        self.saved = []
        return False


def jsonable(value):
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    if isinstance(value, dict):
        return {str(k): jsonable(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [jsonable(v) for v in value]
    raise TypeError("non-JSON runtime value: %r" % (value,))


def envelope(call):
    try:
        runtime = call()
    except Exception as exc:  # noqa: BLE001
        out = {"status": "error", "type": type(exc).__name__, "message": str(exc)}
        code = getattr(exc, "code", None)
        if code:
            out["code"] = code
        return out
    return {"status": "ok", "runtime": jsonable(runtime)}


def expected(r):
    func = r.get("func")
    world = r.get("world") if isinstance(r.get("world"), dict) else {}
    with Patcher(world):
        if func == "azure_foundry":
            return envelope(lambda: live._resolve_azure_foundry_runtime(
                requested_provider=world.get("requested_provider"),
                model_cfg=world.get("model_cfg") or {},
                explicit_api_key=world.get("explicit_api_key"),
                explicit_base_url=world.get("explicit_base_url"),
                target_model=world.get("target_model")))
        if func == "openrouter":
            return envelope(lambda: live._resolve_openrouter_runtime(
                requested_provider=world.get("requested_provider"),
                explicit_api_key=world.get("explicit_api_key"),
                explicit_base_url=world.get("explicit_base_url")))
        if func == "bedrock":
            return envelope(lambda: live._resolve_bedrock_runtime(
                world.get("requested_provider"), world.get("model_cfg") or {},
                world.get("target_model")))
        if func == "is_external":
            return bool(live._is_external_process_provider(world.get("provider")))
        if func == "external_runtime":
            return envelope(lambda: live._resolve_external_process_runtime(
                world.get("provider"), world.get("requested_provider")))
    raise AssertionError("unknown func %r" % (func,))


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
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
