#!/usr/bin/env python3
"""Differential oracle for hermes_cli/config_effective.py (gate w15_configeffe_5314b2).

The live `_effective` is real. Its three collaborators (config._expand_env_vars,
managed_scope.apply_managed_overlay, config._normalize_root_model_keys) are
registered as faithful stubs with the fixture's `env` / `managed` injected, so
no process environment, filesystem, or profile state is read.
"""
import importlib.util
import json
import re
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

CTX = {"env": {}, "managed": {}}
_ENV_RE = re.compile(r"\${([^}]+)}")


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


def _is_non_env_secret_ref(ref):
    return ":" in ref and re.match(r"^[a-z][a-z0-9_-]*:", ref) is not None


def _env_ref_var_name(ref):
    ref = ref.strip()
    if ref.startswith("env:"):
        return ref[len("env:"):].strip() or None
    if _is_non_env_secret_ref(ref):
        return None
    return ref


def _env_expand_match(m):
    raw = m.group(0)
    inner = m.group(1).strip()
    name = _env_ref_var_name(inner)
    if name is None:
        return raw
    val = CTX["env"].get(name)
    return val if val is not None else raw


def _expand_env_vars(obj):
    if isinstance(obj, str):
        return _ENV_RE.sub(_env_expand_match, obj)
    if isinstance(obj, dict):
        return {k: _expand_env_vars(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [_expand_env_vars(x) for x in obj]
    return obj


def _deep_merge(base, override):
    result = base.copy()
    for key, value in override.items():
        over_dict = isinstance(result.get(key), dict)
        if over_dict and isinstance(value, dict):
            result[key] = _deep_merge(result[key], value)
        elif not (over_dict and value is None):
            result[key] = value
    return result


def _coerce_provider_id(value):
    if value is None:
        return ""
    return str(value).strip()


def _normalize_root_model_keys(config):
    model_in = config.get("model")
    model_provider = model_in.get("provider") if isinstance(model_in, dict) else None
    needs_model_work = (model_provider is not None and not isinstance(model_provider, str)) or (
        isinstance(model_in, dict) and (
            model_in.get("api_base") or model_in.get("model") or model_in.get("name")
            or any(isinstance(model_in.get(k), dict) for k in ("default", "model", "name"))))
    has_root = any(config.get(k) for k in ("provider", "base_url", "context_length", "api_base"))
    if not has_root and not needs_model_work:
        return config
    config = dict(config)
    model = config.get("model")
    model = dict(model) if isinstance(model, dict) else ({"default": model} if model else {})
    config["model"] = model
    for _key in ("default", "model", "name"):
        _val = model.get(_key)
        if isinstance(_val, dict):
            _nested_model = _val.get("model") or _val.get("default")
            _nested_provider = str(_val.get("provider") or "").strip()
            model[_key] = str(_nested_model or "").strip()
            if _nested_provider:
                _outer_provider = str(model.get("provider") or "").strip()
                if not _outer_provider or _outer_provider == "auto":
                    model["provider"] = _nested_provider
    for key in ("provider", "base_url", "context_length"):
        root_val = config.get(key)
        if root_val and not model.get(key):
            model[key] = root_val
        config.pop(key, None)
    if model.get("provider") is not None:
        model["provider"] = _coerce_provider_id(model.get("provider"))
    for alias_val in (config.get("api_base"), model.get("api_base")):
        if alias_val and not model.get("base_url"):
            model["base_url"] = alias_val
    config.pop("api_base", None)
    model.pop("api_base", None)
    alias = model.get("model") or model.get("name")
    if not model.get("default") and alias:
        model["default"] = alias
    if model.get("default"):
        model.pop("model", None)
        model.pop("name", None)
    return config


def _apply_managed_overlay(config):
    managed = CTX["managed"]
    if not managed:
        return config
    managed_expanded = _normalize_root_model_keys(_expand_env_vars(managed))
    if isinstance(managed_expanded.get("model"), str):
        managed_expanded = dict(managed_expanded)
        managed_expanded["model"] = {"default": managed_expanded["model"]}
    return _deep_merge(config, managed_expanded)


_mod("hermes_cli.config",
     _expand_env_vars=_expand_env_vars,
     _deep_merge=_deep_merge,
     _normalize_root_model_keys=_normalize_root_model_keys)
_mod("hermes_cli.managed_scope",
     apply_managed_overlay=_apply_managed_overlay,
     load_managed_config=lambda: CTX["managed"])
_mod("hermes_cli.config_backend", read_config_doc=lambda p: None)
_mod("hermes_cli.config_read_errors", _warn_config_parse_failure=lambda *a, **k: None)

SRC = Q + "/hermes_cli/config_effective.py"
spec = importlib.util.spec_from_file_location("hermes_cli.config_effective", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["hermes_cli.config_effective"] = live
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
        if func != "effective":
            continue
        CTX["env"] = r.get("env") or {}
        CTX["managed"] = r.get("managed") or {}
        py = live._effective(r.get("raw"))
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w15_configeffe_5314b2: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
