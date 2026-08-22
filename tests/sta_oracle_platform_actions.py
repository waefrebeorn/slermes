#!/usr/bin/env python3
"""sta_oracle_platform_actions.py — Python twin for
hermes_cli/platform_actions.py, mirrored by src/hermes_cli/platform_actions.c.

The real module's verbs resolve live gateway/consent systems via imports at
call time; this twin drives the LIVE module with those seams monkeypatched to
scripted fakes (same injectable shape the C port exposes), so the gate chain,
error-code contract, per-platform dispatch and audit rule are compared against
the real implementation.

Fixture ops:
  {"kind":"add_reaction"|"set_thread_title", "granted":b, "runner":bool,
   "platform":..., "chat_id","message_id"/"thread_id"/"title",
   "connected":b,"registered":b,"client":bool,"channel_found":bool,
   "fetch_ok":bool,"op_ok":bool}
"""
import asyncio
import importlib.util
import inspect
import json
import sys
import types
from pathlib import Path

_DEV_ROOT = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location(
    "platform_actions_live",
    _DEV_ROOT / "hermes_cli" / "platform_actions.py")
pa = importlib.util.module_from_spec(_spec)
sys.modules["platform_actions_live"] = pa
_spec.loader.exec_module(pa)

KNOWN = ["telegram", "discord", "slack"]
g_env = {"op_ok": True, "channel_found": False, "fetch_ok": True}


class FakeAdapter:
    def __init__(self, platform_value):
        self.platform = types.SimpleNamespace(value=platform_value)
        self.is_connected = True
        if platform_value == "discord":
            outer = self
            class _Client:
                def get_channel(self, channel_id):
                    return outer._channel_proto() if g_env["channel_found"] else None
                async def fetch_channel(self, channel_id):
                    if not g_env["fetch_ok"]:
                        raise RuntimeError("discord api raised")
                    return outer._channel_proto()
                async def fetch_message(self, channel, msg_id):
                    return {"msg": msg_id}
                async def add_reaction(self, emoji):
                    if not g_env["op_ok"]:
                        raise RuntimeError("discord api raised")
            class _Message:
                async def add_reaction(self, emoji):
                    if not g_env["op_ok"]:
                        raise RuntimeError("discord api raised")
            class _Channel:
                async def fetch_message(self, msg_id):
                    return _Message()
            self._client = _Client()
            outer._channel_proto = _Channel
        else:
            self._client = None

    async def _set_reaction(self, chat_id, message_id, emoji):
        return g_env["op_ok"]

    async def rename_dm_topic(self, chat_id, thread_id, title):
        if not g_env["op_ok"]:
            raise RuntimeError("edit_forum_topic raised")
        return None

    async def rename_thread(self, thread_id, title):
        return g_env["op_ok"]

    def get_channel(self, channel_id):
        return {"id": channel_id} if g_env["channel_found"] else None

    async def fetch_channel(self, channel_id):
        if not g_env["fetch_ok"]:
            raise RuntimeError("discord api raised")
        return {"id": channel_id}

    async def fetch_message(self, msg_id):
        return {"msg": msg_id}

    async def add_reaction(self, emoji):
        if not g_env["op_ok"]:
            raise RuntimeError("discord api raised")


def run(coro):
    return asyncio.run(coro)


def main():
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        kind = c.get("kind")
        for k in ("op_ok", "channel_found", "fetch_ok"):
            g_env[k] = c.get(k, {"op_ok": 1, "channel_found": 0,
                                 "fetch_ok": 1}.get(k))
        plat = c.get("platform", "")
        facade = pa.PlatformActions("my-plugin")

        # injectable seams — same shape the C port takes as fn pointers
        facade._capability_granted = (
            lambda: bool(c.get("granted", False))
        )
        adapter = FakeAdapter(plat if plat in KNOWN else "slack")
        runner = {"registered": bool(c.get("registered", True)),
                  "connected": bool(c.get("connected", True)),
                  "client": bool(c.get("client", True))}
        adapter.is_connected = runner["connected"]
        if plat == "discord" and not runner["client"]:
            adapter._client = None

        def _resolve(p):
            if not runner_active[0]:
                return None, pa._err(
                    "gateway_unavailable",
                    "no gateway runner is active in this process")
            name = str(p).strip().lower()
            if name not in KNOWN:
                raise ValueError(name)
            return adapter if runner["registered"] else None

        # _resolve_adapter override — the live method imports gateway.run /
        # gateway.config at call time; inject the scripted registry instead.
        def _resolve(p):
            if not runner_active[0]:
                return None, pa._err(
                    "gateway_unavailable",
                    "no gateway runner is active in this process")
            name = str(p).strip().lower()
            if name not in KNOWN:
                return None, pa._err("unknown_platform",
                                     "unknown platform %r" % p)
            if not runner["registered"]:
                return None, pa._err(
                    "adapter_not_registered",
                    "no %s adapter is registered" % name)
            if not adapter.is_connected:
                return None, pa._err(
                    "adapter_disconnected",
                    "the %s adapter is not connected" % name)
            return adapter, None

        facade._resolve_adapter = _resolve

        runner_active = [bool(c.get("runner", True))]

        verb = kind
        out = None
        audit = ""
        try:
            if verb == "add_reaction":
                res = run(facade.add_reaction(
                    c.get("platform", ""), c.get("chat_id", ""),
                    c.get("message_id", ""), c.get("emoji", "👍")))
            elif verb == "set_thread_title":
                res = run(facade.set_thread_title(
                    c.get("platform", ""), c.get("chat_id", ""),
                    c.get("thread_id", ""), c.get("title", "")))
            else:
                continue
        except Exception:
            # verbs never raise; a leak here is a harness bug
            raise
        audit = ("platform_action plugin=%s verb=%s platform=%s ok=%s" %
                 (facade._plugin_id, kind, c.get("platform", ""),
                  "True" if res.get("ok") else "False"))
        if not res.get("ok"):
            audit += " error=%s" % res.get("error")
        print(json.dumps({"out": res, "audit": audit}, ensure_ascii=False))


if __name__ == "__main__":
    main()
