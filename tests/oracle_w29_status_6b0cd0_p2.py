#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/status.py, gate p2.

Same recipe as p1: the live router is imported by file path with its
dependency set stubbed, so no server, auth store, session store or log file is
touched.  This gate compares the payload shaping and query parsing:

  * ``_webapp_caller_signed_out`` + the signed-out key allowlist projection,
  * ``_count_status_active_sessions``' predicate over fixture session rows,
  * ``_feature_state`` and ``_get_portal_status_sync``,
  * ``get_logs``' parse of file/level/component/lines/search (the ``_read_tail``
    call is recorded and compared, the tail text itself is the seam),
  * ``run_debug_share_endpoint``'s consent gate and line clamp,
  * ``post_health_retirement``'s action/token validation,
  * ``get_curator_status``' ``_safe_call`` defaults, and
  * ``_status_local_reads``' payload.
"""
import asyncio
import contextlib
import importlib.util
import json
import os
import sys
import tempfile
import types
from pathlib import Path
from types import SimpleNamespace as NS

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_status_6b0cd0_p2"

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


class _DebugShareRequest:
    def __init__(self, consent=False, lines=100, redact=True):
        self.consent = consent
        self.lines = lines
        self.redact = redact


class _Any:
    """Accepts any constructor signature (SessionQuery/SessionWindow shapes)."""
    def __init__(self, *a, **k):
        pass


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
            LearningNodeEdit=object, DebugShareRequest=_DebugShareRequest)
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
    _module("hermes_cli.web_server_surface", surface=lambda state: "desktop")
    _module("hermes_cli.auth",
            get_nous_auth_status_local=lambda: {},
            get_nous_session_validity=lambda: "unknown")
    _module("hermes_cli.nous_subscription",
            get_nous_subscription_features=lambda cfg: None)
    _module("hermes_cli.logs", _read_tail=lambda *a, **k: [], LOG_FILES={})
    _module("hermes_logging", COMPONENT_PREFIXES={})
    _module("hermes_cli.debug", build_debug_share=lambda **k: NS(urls=[], failures=[]))
    _module("hermes_cli.debug_redaction", redact_debug_support_text=lambda e: str(e))
    _module("hermes_cli.backend_retirement", retirement=NS())
    _module("agent", curator=NS())
    _module("persistence")
    _module("persistence.conversation")
    _module("persistence.conversation.sessions", SessionQuery=_Any, SessionWindow=_Any)
    _module("persistence_assembly",
            open_persistence_reader=None,
            profile_scope=lambda: NS(profile_dir=Path(".")),
            store_exists=lambda scope: False)


_install_stubs()

_spec = importlib.util.spec_from_file_location(
    "status_router_live_p2", QUARRY / "hermes_cli" / "web_routers" / "status.py")
live = importlib.util.module_from_spec(_spec)
sys.modules["status_router_live_p2"] = live
_spec.loader.exec_module(live)

WANTED = {"signed_out", "signed_out_filter", "active_sessions", "feature_state",
          "portal_status", "log_query", "debug_share", "retirement",
          "curator_status", "local_reads"}


def signed_out(req):
    sys.modules["hermes_cli.web_server_surface"].surface = lambda state: req.get("surface")
    request = NS(app=NS(state=NS(auth_required=bool(req.get("auth_required")))))
    return bool(live._webapp_caller_signed_out(request, bool(req.get("verified"))))


def signed_out_filter(req):
    status = req.get("status") or {}
    return {k: v for k, v in status.items() if k in live._WEBAPP_SIGNED_OUT_STATUS_KEYS}


def active_sessions(req):
    rows = req.get("sessions") or []
    now = req.get("now")
    live.time = NS(time=lambda: now)
    _LATE["_session_db_path_for_profile"] = lambda p: NS(exists=lambda: True)

    def list_sessions(*a, **k):
        return [NS(session=NS(ended_at=r.get("ended_at")), last_active=r.get("last_active"))
                for r in rows]
    store = NS(sessions=NS(list_sessions=list_sessions), close=lambda: None)
    _LATE["_open_session_db_for_profile"] = lambda *a, **k: store
    return live._count_status_active_sessions()


def feature_state(req):
    feat = req.get("feat") or {}
    return live._feature_state(NS(managed_by_nous=feat.get("managed_by_nous", False),
                                  active=feat.get("active", False),
                                  current_provider=feat.get("current_provider")))


def portal_status(req):
    cfg = req.get("cfg")
    auth = req.get("auth")
    feats = req.get("features")
    unavailable = bool(req.get("features_unavailable"))
    _LATE["load_config"] = lambda: cfg
    sys.modules["hermes_cli.auth"].get_nous_auth_status_local = lambda: auth

    def features(_cfg):
        if unavailable:
            raise RuntimeError("portal features unavailable")
        if feats is None:
            return None
        rows = [NS(**f) for f in feats]
        return NS(items=lambda: rows)
    sys.modules["hermes_cli.nous_subscription"].get_nous_subscription_features = features
    return live._get_portal_status_sync()


def log_query(req):
    log_files = req.get("log_files") or {}
    prefixes = req.get("component_prefixes") or {}
    raw = req.get("lines_content") or []
    path_exists = bool(req.get("path_exists", True))
    recorded = {}

    def read_tail(path, n, has_filters=False, min_level=None, component_prefixes=None):
        recorded["tail_n"] = n
        recorded["has_filters"] = has_filters
        recorded["min_level"] = min_level
        recorded["component_prefixes"] = (
            list(component_prefixes) if component_prefixes is not None else None)
        return list(raw)

    _module("hermes_cli.logs", _read_tail=read_tail, LOG_FILES=log_files)
    _module("hermes_logging", COMPONENT_PREFIXES=prefixes)
    base = Path(tempfile.mkdtemp(prefix="w29p2_"))
    if path_exists:
        (base / "logs").mkdir()
        for name in log_files.values():
            if isinstance(name, str) and name:
                (base / "logs" / name).write_text("", encoding="utf-8")
    _LATE["get_hermes_home"] = lambda: base
    _LATE["_config_profile_scope"] = lambda profile: contextlib.nullcontext()
    try:
        res = asyncio.run(live.get_logs(file=req.get("file", "agent"),
                                        lines=req.get("lines", 100),
                                        level=req.get("level"),
                                        component=req.get("component"),
                                        search=req.get("search"),
                                        profile=None))
    except HTTPException as exc:
        return {"error": exc.detail}
    if not recorded:
        return res
    out = dict(res)
    out.update(recorded)
    return out


def debug_share(req):
    recorded = {}

    def build(log_lines=None, redact=True):
        recorded["log_lines"] = log_lines
        return NS(urls=[], failures=[], redacted=False, auto_delete_seconds=0)

    _module("hermes_cli.debug", build_debug_share=build)
    _module("hermes_cli.debug_redaction", redact_debug_support_text=lambda e: str(e))
    body = None
    if req.get("body_present", True):
        body = NS(consent=req.get("consent", False), lines=req.get("lines", 100), redact=True)
    try:
        asyncio.run(live.run_debug_share_endpoint(body=body, profile=None))
    except HTTPException as exc:
        return {"error": exc.status_code, "detail": exc.detail}
    return {"log_lines": recorded.get("log_lines")}


def retirement(req):
    mounted = bool(req.get("mounted"))
    body = req.get("body")
    invalid = bool(req.get("json_invalid"))
    base = req.get("action_result") or {"ok": True}

    class _Request:
        def __init__(self):
            self.app = NS(state=NS(gateway_runner=NS() if mounted else None))

        async def json(self):
            if invalid:
                raise ValueError("bad json")
            return body

    ledger = NS(prepare=lambda: req.get("prepare_result") or {"ok": True},
                commit=lambda token: dict(base, token=token),
                cancel=lambda token: dict(base, token=token))
    _module("hermes_cli.backend_retirement", retirement=ledger)
    _LATE["_require_token"] = lambda r: None
    try:
        res = asyncio.run(live.post_health_retirement(_Request()))
    except HTTPException as exc:
        return {"error": exc.status_code, "detail": exc.detail, "result": None}
    return {"error": None, "detail": None, "result": res}


def curator_status(req):
    spec = req.get("curator") or {}
    raises = set(req.get("raises") or [])

    def accessor(name, value):
        def fn():
            if name in raises:
                raise RuntimeError(name)
            return value
        return fn

    cur = NS()
    for name, value in spec.items():
        if name == "load_state":
            continue
        setattr(cur, name, accessor(name, value))
    cur.load_state = accessor("load_state", spec.get("load_state", {}))
    _module("agent", curator=cur)
    return asyncio.run(live.get_curator_status(profile=None))


def local_reads(req):
    cur, latest = req.get("config_version") or [None, None]
    ver = req.get("version")
    _LATE["check_config_version"] = lambda: (cur, latest)
    _LATE["_dashboard_local_update_managed_externally"] = (
        lambda: bool(req.get("managed_externally")))
    live.get_version_info = lambda: NS(base_version=ver, display_version=ver, commit="c")
    sys.modules["hermes_cli.auth"].get_nous_session_validity = (
        lambda: req.get("nous", "unknown"))
    return live._status_local_reads()


def expected(r):
    f = r.get("func")
    if f == "signed_out":
        return signed_out(r)
    if f == "signed_out_filter":
        return signed_out_filter(r)
    if f == "active_sessions":
        return active_sessions(r)
    if f == "feature_state":
        return feature_state(r)
    if f == "portal_status":
        return portal_status(r)
    if f == "log_query":
        return log_query(r)
    if f == "debug_share":
        return debug_share(r)
    if f == "retirement":
        return retirement(r)
    if f == "curator_status":
        return curator_status(r)
    if f == "local_reads":
        return local_reads(r)
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
