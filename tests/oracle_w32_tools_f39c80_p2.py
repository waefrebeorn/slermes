#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/tools.py — gate w32_tools_f39c80_p2.

The toolsets-matrix / config-panel / write surface.  The live module is
imported BY FILE PATH (its fastapi/pydantic imports are seams) and the routes
are driven for real with their I/O seams patched from the fixture:

  * ``get_toolsets`` — effective toolsets, per-platform tools, key status, the
    GUI label helpers and resolve_toolset all supplied as fixture maps; the
    config-only switch reads the fixture config through the real is_truthy_value
  * ``toggle_toolset`` — the config write / platform-set save and the
    install-on-enable spawn are captured, so the plan itself is compared
  * ``get_toolset_config`` — visible providers, active flags, readiness
    statuses, web capabilities, env presence and the web dispatcher state are
    seams; the row/payload build runs for real
  * ``save_toolset_env`` — the .env get/save seam is a fixture map
  * ``select_toolset_provider`` — the capability path only (a legacy selection
    is delegated whole to apply_provider_selection)
  * ``_dict_section``, the allowed-env-key union and the provider env-key scan

Envelope note: a raise is reported as ``{"ok": false, "status": ..., "detail": ...}``.
"""
import asyncio
import contextlib
import copy
import importlib.util
import json
import os
import sys
import types
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w32_tools_f39c80_p2"
TRUTH = QUARRY / "hermes_cli" / "web_routers" / "tools.py"

os.environ["USERPROFILE"] = os.environ.get("USERPROFILE") or str(REPO)
os.environ["HOME"] = str(REPO)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

sys.path.append(str(Path(__file__).resolve().parent))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

_spec = importlib.util.spec_from_file_location("w32p2_%s" % GATE, TRUTH)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

import hermes_cli.config as hconfig  # noqa: E402
import hermes_cli.nous_subscription as nous  # noqa: E402
import hermes_cli.platforms as platforms  # noqa: E402
import hermes_cli.tools_config as tc  # noqa: E402
import toolsets as toolsets_mod  # noqa: E402

HTTPException = live.HTTPException

WANTED = {
    "dict_section", "toolsets_rows", "allowed_env_keys", "save_env",
    "toggle", "toolset_config", "provider_capability",
}

_MISSING = object()


class Patcher:
    def __init__(self):
        self.saved = []

    def set(self, obj, attr, value):
        self.saved.append((obj, attr, getattr(obj, attr, _MISSING)))
        setattr(obj, attr, value)
        return self

    def close_scopes(self, config):
        self.set(live, "_profile_scope", lambda profile=None: contextlib.nullcontext())
        self.set(live, "config_write_scope", lambda profile=None: contextlib.nullcontext())
        self.set(live, "load_config", lambda: copy.deepcopy(config))
        self.set(live, "save_config", lambda cfg: None)
        return self

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        for obj, attr, old in reversed(self.saved):
            if old is _MISSING:
                delattr(obj, attr)
            else:
                setattr(obj, attr, old)
        return False


def _dict(v):
    return v if isinstance(v, dict) else {}


def _list(v):
    return v if isinstance(v, list) else []


def envelope(fn, *a, **k):
    try:
        return fn(*a, **k)
    except HTTPException as exc:
        return {"ok": False, "status": exc.status_code, "detail": exc.detail}


def _known_rows(known):
    return [(n, n, n) for n in _list(known)]


# ── per-function cases ──────────────────────────────────────────────────────

def dict_section(req):
    cfg = copy.deepcopy(_dict(req.get("config")))
    live._dict_section(cfg, req.get("key"))
    return cfg


def toolsets_rows(req):
    rows = _list(req.get("rows"))
    platform_map = _dict(req.get("platform_map"))
    platform_tools = _dict(req.get("platform_tools"))
    configured = _dict(req.get("configured"))
    gui_labels = _dict(req.get("gui_labels"))
    platform_labels = _dict(req.get("platform_labels"))
    config_only = _list(req.get("config_only"))
    tools_raw = _dict(req.get("tools_raw"))
    tools_error = _list(req.get("tools_error"))

    def _resolve(name):
        if name in tools_error:
            raise LookupError("toolset %r is not registered" % name)
        return list(tools_raw.get(name, []))

    with Patcher() as p:
        p.close_scopes(_dict(req.get("config")))
        p.set(live, "run_in_threadpool", lambda fn, *a, **k: _acall(fn, *a, **k))
        p.set(tc, "_get_effective_configurable_toolsets",
              lambda: [(r.get("name"), r.get("label"), r.get("description")) for r in rows])
        p.set(tc, "_toolset_configuration_platform",
              lambda name, default="cli": platform_map.get(name, "cli"))
        p.set(tc, "_get_platform_tools",
              lambda cfg, platform, include_default_mcp_servers=False: list(
                  platform_tools.get(platform, [])))
        p.set(tc, "_toolset_has_keys",
              lambda name, cfg, features=None: bool(configured.get(name, False)))
        p.set(tc, "get_nous_subscription_features", lambda cfg, force_fresh=False: None)
        p.set(tc, "_CONFIG_ONLY_TOOLSETS", set(config_only))
        p.set(tc, "gui_toolset_label", lambda label: gui_labels.get(label, label))
        p.set(platforms, "platform_label", lambda a, b=None: platform_labels.get(a, a))
        p.set(toolsets_mod, "resolve_toolset", _resolve)
        return asyncio.run(live.get_toolsets(profile=None))


async def _acall(fn, *a, **k):
    return fn(*a, **k)


def allowed_env_keys(req):
    providers = _list(req.get("providers"))
    return sorted({e["key"] for prov in providers for e in prov.get("env_vars", [])})


def save_env(req):
    providers = _list(req.get("providers"))
    env_set = _dict(req.get("env_set"))
    save_errors = _dict(req.get("save_errors"))

    def _save(key, value):
        if key in save_errors:
            raise ValueError(save_errors[key])

    with Patcher() as p:
        p.close_scopes(_dict(req.get("config")))
        p.set(tc, "_get_effective_configurable_toolsets", lambda: _known_rows(req.get("known")))
        p.set(live, "_category_providers", lambda name, config: copy.deepcopy(providers))
        p.set(hconfig, "get_env_value", lambda key: (key if env_set.get(key) else None))
        p.set(hconfig, "save_env_value", _save)
        body = SimpleNamespace(env=dict(_dict(req.get("env_body"))), profile=None)
        return envelope(lambda: asyncio.run(live.save_toolset_env(req.get("name"), body,
                                                                  profile=None)))


def toggle(req):
    name = req.get("name")
    enabled = bool(req.get("enabled"))
    providers = _list(req.get("providers"))
    installed = _list(req.get("installed"))
    config_only = _list(req.get("config_only"))
    platform_map = _dict(req.get("platform_map"))
    platform_tools = _dict(req.get("platform_tools"))
    state = {}

    def _save_config(cfg):
        state["config"] = copy.deepcopy(cfg)

    def _save_platform_tools(cfg, platform, enabled_set):
        state["platform"] = {"platform": platform, "tools": sorted(enabled_set)}

    with Patcher() as p:
        p.close_scopes(_dict(req.get("config")))
        p.set(live, "save_config", _save_config)
        p.set(live, "_spawn_hermes_action", lambda args, kind: None)
        p.set(live, "_profile_cli_args", lambda profile=None: [])
        p.set(live, "_log", SimpleNamespace(exception=lambda *a, **k: None))
        p.set(tc, "_get_effective_configurable_toolsets", lambda: _known_rows(req.get("known")))
        p.set(tc, "_toolset_configuration_platform",
              lambda ts_key, default="cli": platform_map.get(ts_key, "cli"))
        p.set(tc, "_CONFIG_ONLY_TOOLSETS", set(config_only))
        p.set(tc, "_get_platform_tools",
              lambda cfg, platform, include_default_mcp_servers=False: list(
                  platform_tools.get(platform, [])))
        p.set(tc, "_save_platform_tools", _save_platform_tools)
        p.set(tc, "TOOL_CATEGORIES",
              {name: {"providers": providers}} if req.get("has_category") else {})
        p.set(tc, "_visible_providers", lambda cat, cfg, force_fresh=False: providers)
        p.set(tc, "_post_setup_already_installed", lambda key: key in installed)

        body = SimpleNamespace(enabled=req.get("enabled"), profile=None)
        response = envelope(lambda: asyncio.run(live.toggle_toolset(name, body, profile=None)))

    if isinstance(response, dict) and response.get("ok") is False:
        return response
    if name in config_only:
        section = _dict(state.get("config")).get(name) or {}
        saved = {"section": name, "enabled": section.get("enabled")}
    else:
        saved = state.get("platform", {})
    return {"response": response, "saved": saved}


def toolset_config(req):
    name = req.get("name")
    providers = _list(req.get("providers"))
    active_flags = _dict(req.get("active_flags"))
    statuses = _dict(req.get("statuses"))
    caps = _dict(req.get("caps"))
    env_set = _dict(req.get("env_set"))
    web_state = _dict(req.get("web_state"))

    def _caps(backend):
        return list(caps.get(backend) or ["search", "extract"])

    def _status(prov, cfg, *, features=None, is_active=None):
        return statuses.get(prov.get("name"))

    sys.modules["plugins.web.firecrawl.provider"] = types.ModuleType(
        "plugins.web.firecrawl.provider")
    sys.modules["plugins.web.firecrawl.provider"].is_managed_route = (
        lambda cap: bool(web_state.get("is_managed_" + cap, False)))
    fake_web = types.ModuleType("tools.web_tools")
    fake_web._get_search_backend = lambda: web_state.get("search_backend")
    fake_web._get_extract_backend = lambda: web_state.get("extract_backend")
    fake_web._managed_web_search = lambda: web_state.get("managed_search", False)
    sys.modules["tools.web_tools"] = fake_web

    with Patcher() as p:
        p.close_scopes(_dict(req.get("config")))
        p.set(tc, "_get_effective_configurable_toolsets", lambda: _known_rows(req.get("known")))
        p.set(tc, "TOOL_CATEGORIES",
              {name: {"providers": providers}} if req.get("has_category") else {})
        p.set(tc, "_visible_providers", lambda cat, cfg, force_fresh=False: providers)
        p.set(tc, "_is_provider_active",
              lambda prov, cfg, force_fresh=True: bool(active_flags.get(prov.get("name"), False)))
        p.set(tc, "provider_readiness_status", _status)
        p.set(tc, "web_provider_capabilities", _caps)
        p.set(nous, "get_nous_subscription_features", lambda cfg, force_fresh=False: None)
        p.set(hconfig, "get_env_value", lambda key: (key if env_set.get(key) else None))
        return envelope(lambda: asyncio.run(live.get_toolset_config(name, profile=None)))


class _Account:
    def __init__(self, data):
        self.logged_in = bool(data.get("logged_in"))
        self.tool_gateway_entitled = bool(data.get("tool_gateway_entitled"))
        self._per = _dict(data.get("entitled_for"))

    def tool_gateway_entitled_for(self, category):
        return bool(self._per.get(category, False))


class _Features:
    def __init__(self, data):
        account = data.get("account")
        self.account_info = _Account(_dict(account)) if account else None
        self.nous_auth_present = bool(data.get("nous_auth_present", True))


def provider_capability(req):
    name = req.get("name")
    providers = _list(req.get("providers"))
    caps = _dict(req.get("caps"))
    features = _Features(_dict(req.get("features")))
    cap = req.get("capability")
    state = {}

    def _capabilities(backend):
        return list(caps.get(backend) or ["search", "extract"])

    def _save_config(cfg):
        state["config"] = copy.deepcopy(cfg)

    with Patcher() as p:
        p.close_scopes(_dict(req.get("config")))
        p.set(live, "save_config", _save_config)
        p.set(tc, "_get_effective_configurable_toolsets", lambda: _known_rows(req.get("known")))
        p.set(tc, "apply_provider_selection", lambda n, prov, cfg: None)
        p.set(tc, "web_provider_capabilities", _capabilities)
        p.set(live, "_category_providers", lambda n, cfg: copy.deepcopy(providers))
        p.set(nous, "get_nous_subscription_features", lambda cfg, force_fresh=False: features)
        body = SimpleNamespace(capability=cap, provider=req.get("provider"), profile=None)
        response = envelope(lambda: asyncio.run(live.select_toolset_provider(name, body,
                                                                            profile=None)))
    if isinstance(response, dict) and response.get("ok") is False:
        return response
    web = _dict(state.get("config")).get("web")
    key = "%s_backend" % cap
    return {
        "response": response,
        "written": {"section": "web", "key": key,
                    "value": _dict(web).get(key) if isinstance(web, dict) else None},
    }


HANDLERS = {
    "dict_section": dict_section,
    "toolsets_rows": toolsets_rows,
    "allowed_env_keys": allowed_env_keys,
    "save_env": save_env,
    "toggle": toggle,
    "toolset_config": toolset_config,
    "provider_capability": provider_capability,
}


def expected(req):
    handler = HANDLERS.get(req.get("func"))
    if handler is None:
        raise AssertionError("unknown func %r" % (req.get("func"),))
    return handler(req)


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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism or not cases else 0)


if __name__ == "__main__":
    main()
