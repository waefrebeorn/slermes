#!/usr/bin/env python3
"""Differential oracle for gate w14_sessiongro_4089ba (gateway/session_group_peers.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_sessiongro_4089ba"
sys.path.insert(0, QUARRY)
NS = types.SimpleNamespace


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = sys.modules.get(".".join(parts[:i]))
        if parent is not None and not hasattr(parent, parts[i]):
            setattr(parent, parts[i], mod)
    return mod


class AuthorityRefusal(Exception):
    def __init__(self, reason):
        super().__init__(reason)
        self.reason = reason


class HostedRoomPeerError(Exception):
    pass


_seed("gateway")
for _pkg in ("persistence", "persistence.core"):
    if _pkg not in sys.modules:
        _seed(_pkg)
_seed("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)
_seed("gateway.config", Platform=NS(API_SERVER="api_server"))
_seed("gateway.hosted_rooms",
      local_authority_gateway_id=lambda: "gw",
      revoke_room_grant_scope=lambda *a, **kw: None)
_seed("gateway.hosted_room_peer",
      gateway_room_grant_secret_available=lambda: True,
      gateway_room_grant_secret=lambda: "secret",
      HostedRoomPeerError=HostedRoomPeerError,
      decode_room_grant=lambda *a, **k: {})
_seed("gateway.platforms")
_seed("gateway.platforms.api_server_room_grants",
      _local_room_catalog=lambda adapter, profile, gid: (None, {"endpoint": {"available": True}}),
      _issue_invitation=lambda adapter, p, prof: {})
_seed("gateway.session_authorities", served_profile_name=lambda p: "default")
_seed("gateway.session_persistence", authority_persistence=lambda a: object())

SRC = QUARRY + "/gateway/session_group_peers.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)
_ORIG_ROOMLINK = live.room_link
_ORIG_APISERVER = live._api_server


def compute(r):
    func = r.get("func")
    live.room_link = _ORIG_ROOMLINK
    live._api_server = _ORIG_APISERVER
    if func == "target_methods":
        return sorted(live.TARGET_METHODS)
    if func == "room_link":
        profile = r.get("profile", "default")
        grp = sys.modules["gateway.hosted_room_peer"]
        grants = sys.modules["gateway.platforms.api_server_room_grants"]
        sa = sys.modules["gateway.session_authorities"]
        sa.served_profile_name = lambda p: profile
        grp.gateway_room_grant_secret_available = lambda: bool(r.get("secret", True))
        catalog = r.get("catalog") or {"endpoint": {"available": True}}
        def catalog_fn(adapter, prof, gid):
            if r.get("catalog_raises"):
                raise RuntimeError("no")
            return (None, catalog)
        grants._local_room_catalog = catalog_fn
        adapters = {}
        if r.get("adapter"):
            adapters["api_server"] = NS(_run_idempotency_store=NS(durable=bool(r.get("durable", True))))
        auth = NS(profile_id="/h", runner=NS(adapters=adapters))
        return live.room_link(auth)
    if func == "invite":
        params = dict(r.get("params") or {})
        ep = params.get("authority_epoch")
        if isinstance(ep, str):
            try:
                params["authority_epoch"] = int(ep)
            except ValueError:
                pass
        live.room_link = lambda a: {"enabled": bool(r.get("link_enabled", False)),
                                    "profile": r.get("profile", "default")}
        live._api_server = lambda a: NS()
        grants = sys.modules["gateway.platforms.api_server_room_grants"]
        grants._issue_invitation = lambda adapter, p, prof: r.get("invitation") or {}
        try:
            return live._invite(NS(), params)
        except AuthorityRefusal as exc:
            return {"error": exc.reason}
    if func == "revoke":
        grp = sys.modules["gateway.hosted_room_peer"]
        claims = r.get("claims") or {}
        def dec(*a, **k):
            if r.get("decode_raises"):
                raise HostedRoomPeerError("bad")
            return claims
        grp.decode_room_grant = dec
        sys.modules["gateway.session_authorities"].served_profile_name = lambda p: r.get("profile", "default")
        sys.modules["gateway.hosted_rooms"].local_authority_gateway_id = lambda: r.get("gateway_id", "gw")
        try:
            return live._revoke(NS(profile_id="/h"), {"grant": r.get("grant")})
        except AuthorityRefusal as exc:
            return {"error": exc.reason}
    return None


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
