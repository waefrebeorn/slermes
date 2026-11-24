#!/usr/bin/env python3
"""Differential oracle for gateway/session_route_recovery.py (w18_sessionrou_7912bd)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# parent-package bootstrap
for pkg in ("gateway",):
    if pkg not in sys.modules:
        _mk(pkg)


class Platform:
    _vals = dict(local="local", telegram="telegram", discord="discord",
                 whatsapp="whatsapp", whatsapp_cloud="whatsapp_cloud", slack="slack",
                 signal="signal", mattermost="mattermost", matrix="matrix", email="email",
                 sms="sms", dingtalk="dingtalk", api_server="api_server", webhook="webhook",
                 msgraph_webhook="msgraph_webhook", feishu="feishu", wecom="wecom",
                 wecom_callback="wecom_callback", weixin="weixin", bluebubbles="bluebubbles",
                 qqbot="qqbot", relay="relay")

    def __init__(self, v):
        if v not in self._vals:
            raise ValueError(v)
        self.value = self._vals[v]

    def __eq__(self, o):
        return isinstance(o, Platform) and self.value == o.value

    def __hash__(self):
        return hash(self.value)


Platform.LOCAL = Platform("local")
Platform.API_SERVER = Platform("api_server")
Platform.TELEGRAM = Platform("telegram")

_mk("gateway.config", Platform=Platform)
_mk("gateway.session_persistence", authority_persistence=lambda a: None, owner_rule=lambda: None)
_mk("persistence")
_mk("persistence.authority")
_mk("persistence.authority.vocabulary", LEGACY_LOCAL_SOURCES=set())

SRC = Q + "/gateway/session_route_recovery.py"
spec = importlib.util.spec_from_file_location("w18_sessionrou_7912bd_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def compat(v):
    return None if v is None else getattr(v, "value", v)


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "messaging_platform":
        continue
    try:
        py = compat(live._messaging_platform(r.get("label")))
    except Exception as exc:
        print(f"LIVE ERROR messaging_platform {r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH messaging_platform args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w18_sessionrou_7912bd: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
