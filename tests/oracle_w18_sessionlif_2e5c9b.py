#!/usr/bin/env python3
"""Differential oracle for gate w18_sessionlif_2e5c9b (tui_gateway/session_lifecycle.py)."""
import contextlib
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = [Q + "/" + name.replace(".", "/")]
    sys.modules[name] = m


_pkg("tui_gateway")
_pkg("gateway")
ctx = types.ModuleType("tui_gateway.method_ctx")


class HandlerRegistry:
    def __init__(self):
        self._pending = []

    def method(self, name):
        def dec(fn):
            self._pending.append((name, fn))
            return fn
        return dec

    def profile_scoped(self, fn):
        return fn


ctx.HandlerRegistry = HandlerRegistry
ctx.bind_module = lambda *a, **k: None
sys.modules["tui_gateway.method_ctx"] = ctx

gc = types.ModuleType("gateway.config")
PLATFORM_VALUES = {
    "local", "telegram", "discord", "whatsapp", "whatsapp_cloud", "slack", "signal",
    "mattermost", "matrix", "email", "sms", "dingtalk", "api_server", "webhook",
    "msgraph_webhook", "feishu", "wecom", "wecom_callback", "weixin", "bluebubbles",
    "qqbot", "relay"}


class Platform:
    def __init__(self, value):
        if value not in PLATFORM_VALUES:
            raise ValueError(value)


gc.Platform = Platform
sys.modules["gateway.config"] = gc

NAME = "tui_gateway.session_lifecycle_live"
spec = importlib.util.spec_from_file_location(
    NAME, Q + "/tui_gateway/session_lifecycle.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)
live._sessions = {}
live._sessions_lock = contextlib.nullcontext()

G = "w18_sessionlif_2e5c9b"


def compute(r):
    func = r.get("func")
    inp = r.get("in") or {}
    if func == "lease_metadata":
        return live._lease_metadata(inp.get("live_session_id", ""))
    if func == "is_gateway_owned_source":
        return bool(live._is_gateway_owned_source(inp.get("source", "")))
    if func == "lifecycle_own_sid":
        return live._lifecycle_own_sid(inp.get("session") or {}, inp.get("sid_hint", ""))
    if func == "session_holds_transcript_claim":
        return bool(live._session_holds_transcript_claim(inp.get("session") or {}, inp.get("now", 0.0)))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print(f"ORACLE {G}: live raised {exc!r} case={r.get('case')}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {G} {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
