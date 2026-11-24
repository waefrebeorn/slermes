#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/status.py, gate p1.

The live router module is imported BY FILE PATH from the quarry tree with its
dependency set stubbed: fastapi/starlette (no server), hermes_cli.web_deps
(late-bound seams become fixture lookups), gateway.status (the liveness ladder
becomes fixture values), the session store, the config/auth stores and the FTS
reader.  Nothing binds a port, opens state.db or starts a thread.

What is compared is the value work the /api/status handler does on those
probes' results:

  * ``_is_profile_platform_status_key`` / ``_status_platform_key_allowed``,
  * ``_public_platform_entry`` (writer stamps stripped),
  * ``_merge_profile_gateway_platforms`` (multiplexed profiles folded in),
  * ``_project_gateway_platforms`` (stale / startup_failed clearing),
  * ``_resolve_gateway_status``' state/staleness/served_profiles ladder,
  * ``_component_health`` and the platform counts it rolls up,
  * the inline ``overall`` verdict, and
  * ``_fts_rebuild_status`` progress.
"""
import asyncio
import importlib.util
import json
import os
import sys
import types
from pathlib import Path
from types import SimpleNamespace as NS

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_status_6b0cd0_p1"

if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(QUARRY.parent)

_LATE = {}


class HTTPException(Exception):
    def __init__(self, status_code=500, detail=None):
        super().__init__(detail)
        self.status_code = status_code
        self.detail = detail


class _APIRouter:
    def _deco(self, *a, **k):
        def wrap(fn):
            return fn
        return wrap
    get = post = put = delete = patch = _deco


class _LateProxy:
    def __init__(self, name):
        self._name = name

    def __call__(self, *a, **k):
        fn = _LATE.get(self._name)
        if fn is None:
            raise RuntimeError("late seam unset: %s" % self._name)
        return fn(*a, **k)

    def __getattr__(self, item):
        return None


class _LateState:
    def __init__(self, name):
        self.name = name
        self.state = NS()


def late(name, module=None):
    return _LateProxy(name)


async def _run_in_threadpool(fn, *a, **k):
    return fn(*a, **k)


async def _config_scoped(profile, fn):
    return fn()


async def _scoped(profile, fn):
    return fn()


def _module(name, **attrs):
    m = sys.modules.get(name) or types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    if not hasattr(m, "__path__"):
        m.__path__ = []
    sys.modules[name] = m
    return m


def _install_stubs():
    _module("fastapi", APIRouter=_APIRouter, HTTPException=HTTPException, Request=object)
    _module("starlette")
    _module("starlette.concurrency", run_in_threadpool=_run_in_threadpool)
    _module("hermes_cli", __release_date__="2026-11-15")
    _module("hermes_cli.web_deps", LateState=_LateState, late=late)
    _module("hermes_cli.web_server_gateway",
            _display_system_platform=lambda **k: {},
            _collect_profile_gateway_topology_cached=lambda: {},
            _load_configured_gateway_platforms=lambda: None,
            _probe_gateway_health=lambda: (False, None),
            _spawn_hermes_action=lambda *a, **k: NS(pid=0))
    _module("gateway")
    _module("gateway.status",
            derive_gateway_busy=lambda **k: False,
            derive_gateway_drainable=lambda **k: True,
            normalize_updated_at=lambda v: v if isinstance(v, str) else None,
            parse_active_agents=lambda v: v if isinstance(v, (int, float)) else 0,
            profile_platforms_from_multiplexer=lambda rt, name: {},
            resolve_gateway_liveness=lambda **k: None,
            retained_gateway_state=lambda rt: None,
            runtime_status_heartbeat_age_s=lambda rt: None,
            runtime_status_is_stale=lambda rt: False)
    _module("gateway.status_served", local_liveness_pid=lambda lv: None)
    _module("gateway.readiness", _probe_state_db=lambda home: {})
    _module("hermes_cli.config",
            get_config_path=lambda: Path("config.yaml"),
            get_env_path=lambda: Path(".env"),
            get_hermes_home=lambda: Path("."),
            check_config_version=lambda: (0, 0),
            load_config=lambda: {})
    _module("hermes_cli.isolated_scope",
            isolated_launch_profile=lambda: None,
            scope_to_launch_profile=lambda v: v)
    _module("hermes_cli.version_info",
            get_version_info=lambda: NS(base_version="0.9.0", display_version="0.9.0", commit="abc"))
    _module("hermes_constants",
            get_process_hermes_home=lambda: Path("."),
            profile_name_for_home=lambda h: "default")
    _module("hermes_cli.web_models", CuratorPause=object, LearningNodeRef=object,
            LearningNodeEdit=object, DebugShareRequest=object)
    _module("hermes_cli.web_routers")
    _module("hermes_cli.web_routers._common",
            config_scoped_to_thread=_config_scoped,
            destructive_profile=lambda profile, where: profile,
            scoped_to_thread=_scoped)
    _module("hermes_cli.web_routers.status_public",
            redact_status_for_unverified=lambda s: s)
    _module("hermes_cli.profiles", get_active_profile_name=lambda: "default")
    _module("hermes_cli.web_server",
            DASHBOARD_HEALTH=NS(snapshot=lambda: {}),
            _require_token=lambda r: None,
            _ssh_runtime_owner_nonce="",
            _ssh_runtime_intact=lambda: True,
            _GATEWAY_HEALTH_URL=None)
    _module("persistence_assembly",
            open_persistence_reader=None,
            profile_scope=lambda: NS(profile_dir=Path(".")),
            store_exists=lambda scope: False)


_install_stubs()

_spec = importlib.util.spec_from_file_location(
    "status_router_live", QUARRY / "hermes_cli" / "web_routers" / "status.py")
live = importlib.util.module_from_spec(_spec)
sys.modules["status_router_live"] = live
_spec.loader.exec_module(live)

WANTED = {"is_key", "key_allowed", "public_entry", "merge", "project",
          "gateway_view", "component_health", "overall", "fts_rebuild"}


def _configured(req):
    c = req.get("configured")
    return None if c is None else set(c)


def is_key(req):
    return bool(live._is_profile_platform_status_key(req.get("key")))


def key_allowed(req):
    return bool(live._status_platform_key_allowed(req.get("key"), _configured(req)))


def public_entry(req):
    return live._public_platform_entry(req.get("value"))


def merge(req):
    sys.modules["hermes_cli.profiles"].get_active_profile_name = lambda: req.get("active")
    return live._merge_profile_gateway_platforms(req.get("gateway_platforms") or {},
                                                 req.get("profile_platforms") or {})


def project(req):
    return live._project_gateway_platforms(req.get("gateway_platforms") or {},
                                           _configured(req),
                                           bool(req.get("gateway_running")),
                                           req.get("gateway_state"))


async def _gateway_view(req):
    runtime = req.get("local_runtime")
    remote = req.get("remote_body")
    running = bool(req.get("gateway_running"))
    _LATE["read_runtime_status"] = lambda **k: runtime
    _LATE["_load_configured_gateway_platforms"] = (
        (lambda: None) if req.get("configured_error") else (lambda: _configured(req)))
    sys.modules["gateway.status"].resolve_gateway_liveness = (
        lambda **k: NS(running=running, health_body=remote, pid=req.get("pid"),
                       runtime=None, liveness_pid=None))
    live.resolve_gateway_liveness = sys.modules["gateway.status"].resolve_gateway_liveness
    live.local_liveness_pid = lambda lv: None
    live.retained_gateway_state = lambda rt: req.get("retained_state")
    live.runtime_status_is_stale = lambda rt: bool(req.get("stale"))
    live.runtime_status_heartbeat_age_s = lambda rt: req.get("heartbeat_age")
    res = await live._resolve_gateway_status(None, None)
    return {k: res[k] for k in ("gateway_running", "gateway_pid", "gateway_state",
                                "gateway_platforms", "gateway_exit_reason",
                                "gateway_heartbeat_stale_s", "gateway_shared_with")}


def gateway_view(req):
    return asyncio.run(_gateway_view(req))


async def _component_health(req):
    dash = req.get("dashboard")
    storage = req.get("storage")
    err = bool(req.get("storage_error"))
    sys.modules["hermes_cli.web_server"].DASHBOARD_HEALTH = NS(snapshot=lambda: dash)

    def probe(home):
        if err:
            raise RuntimeError("storage unavailable")
        return storage
    sys.modules["gateway.readiness"]._probe_state_db = probe
    _LATE["get_hermes_home"] = lambda: Path(".")
    gateway = {"gateway_running": bool(req.get("gateway_running")),
               "gateway_state": req.get("gateway_state"),
               "gateway_platforms": req.get("gateway_platforms") or {}}
    return await live._component_health(gateway)


def component_health(req):
    return asyncio.run(_component_health(req))


def overall(req):
    comps = req.get("components") or {}
    return "ok" if all(item.get("status") == "ok" for item in comps.values()) else "degraded"


def fts_rebuild(req):
    store_exists = bool(req.get("store_exists"))
    rebuild = req.get("rebuild")

    class _Store:
        def __enter__(self):
            return self

        def __exit__(self, *a):
            return False

        @property
        def search(self):
            def status():
                if rebuild is None:
                    return None
                return NS(total=rebuild.get("total"), indexed=rebuild.get("indexed"))
            return NS(search_index_rebuild_status=status)

    pa = sys.modules["persistence_assembly"]
    pa.store_exists = lambda scope: store_exists
    pa.profile_scope = lambda: NS(profile_dir=Path("."))
    pa.open_persistence_reader = lambda d: _Store()
    return live._fts_rebuild_status()


def expected(r):
    f = r.get("func")
    if f == "is_key":
        return is_key(r)
    if f == "key_allowed":
        return key_allowed(r)
    if f == "public_entry":
        return public_entry(r)
    if f == "merge":
        return merge(r)
    if f == "project":
        return project(r)
    if f == "gateway_view":
        return gateway_view(r)
    if f == "component_health":
        return component_health(r)
    if f == "overall":
        return overall(r)
    if f == "fts_rebuild":
        return fts_rebuild(r)
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
