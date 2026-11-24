#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_server_dashboard.py — gate
w28_webserverd_919ecc_p2 (dashboard-plugin manifests + plugin-API trust gates).

The live module is imported BY FILE PATH behind the repo's seam stubs.  Every
I/O seam is a fixture value:

  * the plugin filesystem walk -> ``_dashboard_plugin_entry``'s data/name/dir;
  * the FastAPI app + its route table -> a fake app holding ``_Route`` objects
    (``endpoint is _plugin_api_hot_mount`` marks the hot-mount fallback);
  * ``hermes_cli.plugin_isolation`` -> a fake whose ``isolation_mode`` and
    ``import_user_code`` answer the fixture (and record the call);
  * ``tools.registry`` (cached check_fn verdicts) and
    ``hermes_cli.left_core_migration.LEFT_CORE`` -> fixture tables;
  * ``load_config_readonly`` -> the fixture config;
  * ``starlette._utils.get_route_path`` -> its canonical six lines (the stub
    replaces it with a decorator; that function is framework, not policy).

Called live: ``_strip_dashboard_manifest``, ``_plugin_runtime_status``,
``_plugin_api_mount_skip_reason``, ``_dashboard_plugin_entry``,
``_safe_plugin_api_relpath``, ``_plugin_api_name``, ``_plugin_api_mounted``,
``_plugin_login_command``, ``_plugin_auth_hint``, ``_plugin_enable_sets_here``
and the whole ``_mount_one_plugin_api`` ladder.

Mirrored statement-for-statement (inline slices with no callable body): the
``_try_hot_mount_plugin_api`` name guard, the hub memo check, the route splice
index and the orphan-dashboard comprehension.  Their constants come from the
live module.
"""
import importlib.util
import json
import os
import sys
import tempfile
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_webserverd_919ecc_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

if os.name == "nt" and not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)

sys.path.append(str(Path(__file__).resolve().parent))
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()


def _load(name, rel):
    spec = importlib.util.spec_from_file_location(name, QUARRY / rel)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


live = _load("w28_dash_p2_live", "hermes_cli/web_server_dashboard.py")

# The module's warnings ARE the refusal reasons the port reproduces; the gate reads
# the decisions, not the log lines, so keep stdout/stderr for mismatches only.
import logging  # noqa: E402

live._log.addHandler(logging.NullHandler())
live._log.propagate = False
live._log.setLevel(logging.CRITICAL)

import hermes_cli  # noqa: E402  (real package: the quarries' dotted tail)
import hermes_cli.config as _cfgmod  # noqa: E402

WANTED = {
    "strip_manifest", "runtime_status", "mount_skip_reason", "plugin_entry",
    "safe_api_relpath", "api_name", "api_mounted", "hot_name_ok",
    "login_command", "auth_hint", "mount_decision", "env_truthy",
    "enable_sets", "orphan_dashboard", "cache_hit", "splice_index", "constants",
}

# --- the framework seam: starlette._utils.get_route_path, verbatim ----------
def _get_route_path(scope):
    path = scope["path"]
    root_path = scope.get("root_path", "")
    if not root_path:
        return path
    if not path.startswith(root_path):
        return path
    return path[len(root_path):]


live.get_route_path = _get_route_path

# --- fake app / routes ------------------------------------------------------
_HOT = live._plugin_api_hot_mount


class _Route:
    def __init__(self, path, hot=False):
        self.path = path
        self.endpoint = _HOT if hot else (lambda: None)


class _Router:
    def __init__(self):
        self.routes = []


class _App:
    def __init__(self, routes=()):
        self.router = _Router()
        self.router.routes = list(routes)
        self.calls = []

    def include_router(self, router, **kw):
        self.calls.append(("include_router", kw.get("prefix")))

    def add_api_route(self, path, fn, **kw):
        self.calls.append(("add_api_route", path))


def _routes(raw):
    return [_Route(r.get("path", ""), bool(r.get("hot"))) for r in (raw or [])]


# --- seam modules -----------------------------------------------------------
iso = types.ModuleType("hermes_cli.plugin_isolation")
iso.ISOLATION_HOST = "host"
iso.isolation_mode = lambda: "in-process"
iso.import_user_code = lambda path, name: types.SimpleNamespace(router=object())
sys.modules["hermes_cli.plugin_isolation"] = iso
hermes_cli.plugin_isolation = iso

left_core_mod = types.ModuleType("hermes_cli.left_core_migration")
left_core_mod.LEFT_CORE = []
sys.modules["hermes_cli.left_core_migration"] = left_core_mod
hermes_cli.left_core_migration = left_core_mod


class _Entry:
    def __init__(self, check_fn):
        self.check_fn = check_fn


class _Registry:
    def __init__(self):
        self.entries = {}

    def get_entry(self, name):
        return self.entries.get(name)


registry_mod = types.ModuleType("tools.registry")
registry_mod.registry = _Registry()
registry_mod.get_cached_check_fn_result = lambda fn: None
tools_mod = types.ModuleType("tools")
tools_mod.registry = registry_mod
sys.modules["tools"] = tools_mod
sys.modules["tools.registry"] = registry_mod

_TMPROOT = Path(tempfile.mkdtemp(prefix="w28dash_p2_"))
_case_no = [0]


# --- per-function expectations ---------------------------------------------
def strip_manifest(r):
    return live._strip_dashboard_manifest(r.get("p") or {})


def runtime_status(r):
    return live._plugin_runtime_status(set(r.get("aliases") or []),
                                      set(r.get("enabled") or []),
                                      set(r.get("disabled") or []))


def mount_skip_reason(r):
    return live._plugin_api_mount_skip_reason(r.get("plugin") or {},
                                              set(r.get("enabled") or []),
                                              set(r.get("disabled") or []))


def plugin_entry(r):
    out = live._dashboard_plugin_entry(r.get("data") or {}, r.get("name", ""),
                                       Path(r.get("dashboard_dir", "")), r.get("source", ""))
    if isinstance(out.get("_dir"), str):
        out["_dir"] = out["_dir"].replace(os.sep, "/")
    return out


def safe_api_relpath(r):
    return live._safe_plugin_api_relpath(r.get("api_field"),
                                         dashboard_dir=Path(r.get("dashboard_dir", "")))


def api_name(r):
    return live._plugin_api_name({"path": r.get("path", ""), "root_path": r.get("root_path", "")})


def api_mounted(r):
    return live._plugin_api_mounted(_App(_routes(r.get("routes"))), r.get("name", ""))


def hot_name_ok(r):
    # _try_hot_mount_plugin_api's guard: if not name or any(c in name for c in "/\\{}") or name == ".."
    name = r.get("name", "")
    return not (not name or any(c in name for c in "/\\{}") or name == "..")


def login_command(r):
    left_core_mod.LEFT_CORE = [types.SimpleNamespace(plugin=e.get("plugin"), cli=e.get("cli"))
                               for e in (r.get("left_core") or [])]
    return live._plugin_login_command(r.get("name", ""))


def auth_hint(r):
    reg = _Registry()
    for tool, ent in (r.get("registry") or {}).items():
        reg.entries[tool] = _Entry(ent.get("check_fn") if isinstance(ent, dict) else None)
    cached = dict(r.get("cached") or {})
    registry_mod.registry = reg
    registry_mod.get_cached_check_fn_result = lambda fn: cached.get(fn)
    left_core_mod.LEFT_CORE = [types.SimpleNamespace(plugin=e.get("plugin"), cli=e.get("cli"))
                               for e in (r.get("left_core") or [])]
    probes = []
    live._schedule_check_fn_probe = lambda fn: (probes.append(fn), None)[1]
    required, command = live._plugin_auth_hint(r.get("name", ""), list(r.get("provides_tools") or []))
    return {"auth_required": required, "auth_command": command, "probes": probes}


def mount_decision(r):
    """The _mount_one_plugin_api ladder against a real (temp) plugin tree."""
    plugin = dict(r.get("plugin") or {})
    _case_no[0] += 1
    base = _TMPROOT / ("c%03d" % _case_no[0])
    dash = base / "plug" / "dashboard"
    dash.mkdir(parents=True, exist_ok=True)
    api_file = plugin.get("_api_file")
    if api_file:
        if not r.get("api_inside", True):
            (dash.parent / "escape.py").write_text("router = 1\n", encoding="utf-8")
            api_file = "../escape.py"
        elif r.get("api_exists", True):
            target = (dash / api_file)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text("router = 1\n", encoding="utf-8")
        plugin["_api_file"] = api_file
        plugin["_dir"] = str(dash)
    iso.isolation_mode = lambda: r.get("isolation", "in-process")
    calls = []

    def _import(path, modname):
        calls.append(modname)
        mode = r.get("import_mode", "ok")
        if mode == "none":
            return None
        if mode == "raise":
            raise RuntimeError("probe import failure")
        if mode == "norouter":
            return types.SimpleNamespace(norouter=1)
        return types.SimpleNamespace(router=object())

    iso.import_user_code = _import
    app = _App(_routes(r.get("routes")))
    ok = live._mount_one_plugin_api(app, plugin, set(r.get("enabled") or []),
                                    set(r.get("disabled") or []))
    forwarded = any(kind == "add_api_route" for kind, _ in app.calls)
    return {"ok": bool(ok),
            "mode": "hosted" if forwarded else ("in-process" if ok else "none"),
            "imported": bool(calls),
            "forwarded": forwarded}


def env_truthy(r):
    value = r.get("value")
    if value is None:
        os.environ.pop("HERMES_ENABLE_PROJECT_PLUGINS", None)
    else:
        os.environ["HERMES_ENABLE_PROJECT_PLUGINS"] = value
    return live.env_var_enabled("HERMES_ENABLE_PROJECT_PLUGINS")


def enable_sets(r):
    config = r.get("config") or {}
    _cfgmod.load_config_readonly = lambda: config
    enabled, disabled = live._plugin_enable_sets_here()
    return {"enabled": sorted(enabled), "disabled": sorted(disabled)}


def orphan_dashboard(r):
    rows = r.get("rows") or []
    agent_names = {row.get("name") for row in rows}
    return [live._strip_dashboard_manifest(p) for p in (r.get("dashboard_list") or [])
            if str(p.get("name")) not in agent_names]


def cache_hit(r):
    # if not force_refresh: cached is not None and now < expires.get(key, 0.0)
    return bool(not r.get("force") and r.get("has_entry") and
                float(r.get("now") or 0.0) < float(r.get("expiry") or 0.0))


def splice_index(r):
    routes = r.get("routes") or []
    return next((i for i, route in enumerate(routes) if route.get("hot")), len(routes))


def constants():
    base = live._dashboard_plugin_entry({}, "x", Path("d"), "")
    disabled_reason = live._plugin_api_mount_skip_reason({"name": "p", "source": "bundled"}, set(), {"p"})
    not_enabled_reason = live._plugin_api_mount_skip_reason({"name": "p", "source": "user"}, set(), set())
    auth_prefix = live._plugin_login_command("zzz")[:-len("zzz")]
    return {
        "hub_cache_ttl_seconds": live._PLUGINS_HUB_CACHE_TTL_SECONDS,
        "skip_reason_disabled": disabled_reason,
        "skip_reason_not_enabled": not_enabled_reason,
        "isolation_host": iso.ISOLATION_HOST,
        "default_entry": base["entry"],
        "default_icon": base["icon"],
        "default_version": base["version"],
        "default_tab_position": base["tab"]["position"],
        "auth_command_prefix": auth_prefix,
    }


def expected(r):
    f = r.get("func")
    if f == "strip_manifest":
        return strip_manifest(r)
    if f == "runtime_status":
        return runtime_status(r)
    if f == "mount_skip_reason":
        return mount_skip_reason(r)
    if f == "plugin_entry":
        return plugin_entry(r)
    if f == "safe_api_relpath":
        return safe_api_relpath(r)
    if f == "api_name":
        return api_name(r)
    if f == "api_mounted":
        return api_mounted(r)
    if f == "hot_name_ok":
        return hot_name_ok(r)
    if f == "login_command":
        return login_command(r)
    if f == "auth_hint":
        return auth_hint(r)
    if f == "mount_decision":
        return mount_decision(r)
    if f == "env_truthy":
        return env_truthy(r)
    if f == "enable_sets":
        return enable_sets(r)
    if f == "orphan_dashboard":
        return orphan_dashboard(r)
    if f == "cache_hit":
        return cache_hit(r)
    if f == "splice_index":
        return splice_index(r)
    if f == "constants":
        return constants()
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
            py = expected(r)
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
