#!/usr/bin/env python3
"""Differential oracle for gateway/plugin_keyless_inject.py (w17_w17_pluginkeyl_d5f537_d5f537)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# Parent-package bootstrap + Platform stub (the live module imports it at module scope).
gateway = _mod("gateway")
gateway.__path__ = []
class Platform:
    LOCAL = "local"
    RELAY = "relay"
    def __init__(self, value): self.value = value
_mod("gateway.config", Platform=Platform)

_state = {}
class AuthorityRefusal(Exception):
    pass

def authorize_local_source(runner, source):
    # The live seam is asked for `is True`; feed it the exact bool from the fixture.
    return source.authorized

def policy_for_source(runner, source):
    if source.policy_refusal:
        raise AuthorityRefusal()
    return None if source.policy_source is None else types.SimpleNamespace(source=source.policy_source)

def authority_for_home(runner, plugin_home):
    return _state.get("authority")

def all_authorities(runner):
    return _state.get("authorities", [])

_mod("gateway.session_local", authorize_local_source=authorize_local_source)
_mod("gateway.session_policy", policy_for_source=policy_for_source)
_mod("gateway.session_authorities",
     authority_for_home=authority_for_home,
     all_authorities=all_authorities,
     active_authority=lambda runner: None)
persistence = _mod("persistence"); persistence.__path__ = []
pcore = _mod("persistence.core"); pcore.__path__ = []
_mod("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)

SRC = Q + "/gateway/plugin_keyless_inject.py"
spec = importlib.util.spec_from_file_location("w17_pluginkeyl_d5f537_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w17_pluginkeyl_d5f537_live"] = live
spec.loader.exec_module(live)


class Src:
    def __init__(self, d):
        d = d or {}
        self.platform = d.get("platform")
        self.authorized = d.get("authorized") is True
        self.policy_refusal = d.get("policy_refusal", False)
        self.policy_source = d.get("policy_source")


class Sess:
    def __init__(self, d):
        sd = d.get("source")
        self.route = d.get("route", "")
        self.source = Src(sd) if isinstance(sd, dict) else None
        self.subscribers = bool(d.get("subscribers", False))


class Auth:
    def __init__(self, sessions, ident):
        self.sessions = {s.route: s for s in sessions}
        self.id = ident
        self.profile_id = ident
        self.runner = object()


runner = object()
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
        if func == "local":
            fake = types.SimpleNamespace(source=Src(r.get("source")))
            py = live._local(runner, fake)
        elif func == "launched":
            fake = types.SimpleNamespace(source=Src(r.get("source")))
            py = live._launched_by_classic_cli(runner, fake)
        elif func == "resolve":
            ph = r.get("plugin_home", None)
            own = r.get("own_sessions", [])
            other = r.get("other_sessions", [])
            authority = Auth([Sess(x) for x in own], "own") if (ph is not None and r.get("served", False)) else None
            others = [Auth([Sess(x) for x in other], "other")]
            _state["authority"] = authority
            _state["authorities"] = others + ([authority] if authority is not None else [])
            res = live.resolve(runner, plugin_home=ph,
                               calling_session_key=r.get("calling_session_key", ""),
                               plugin_id=r.get("plugin_id", "plug"))
            if res is None:
                py = None
            else:
                a, s = res
                py = {"authority": a.id, "route": s.route}
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w17_pluginkeyl_d5f537: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
