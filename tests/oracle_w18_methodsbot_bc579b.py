#!/usr/bin/env python3
"""Differential oracle for gate w18_methodsbot_bc579b (tui_gateway/methods_bot_relay.py)."""
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
_pkg("tools")
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

STATE = {}


def _write_reply(root, envelope_id, reply="", error="", reason=""):
    mode = STATE.get("mode", "ok")
    if mode == "valueerror":
        raise ValueError(STATE.get("msg", "bad"))
    if mode == "error":
        raise RuntimeError(STATE.get("msg", "boom"))
    STATE["written"] = {"id": envelope_id, "reply": reply, "error": error, "reason": reason}


brelay = types.ModuleType("tools.bot_relay")
brelay.write_reply = _write_reply
brelay.qualify_sender_stamp = lambda msg, *a: msg
brelay.read_remote_roster = lambda root: []
sys.modules["tools.bot_relay"] = brelay
bprobe = types.ModuleType("tools.bot_mode_probe")
bprobe._default_home = lambda: "/home/hermes"
bprobe._hermes_root = lambda p: "ROOT"
bprobe.local_taken_forms = lambda root: []
bprobe._profile_for_handle = lambda h, roster: h
bprobe._roster = lambda root: {}
sys.modules["tools.bot_mode_probe"] = bprobe

NAME = "tui_gateway.methods_bot_relay_live"
spec = importlib.util.spec_from_file_location(
    NAME, Q + "/tui_gateway/methods_bot_relay.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

live._ok = lambda rid, payload: payload
live._err = lambda rid, code, msg, data=None: {"code": code, "message": msg, "data": data}
live.current_transport = lambda: None
_reply = next(fn for name, fn in live._registry._pending if name == "bot_relay.reply")

G = "w18_methodsbot_bc579b"


def compute(r):
    if r.get("func") != "reply":
        return None
    STATE.clear()
    STATE.update(r.get("write") or {})
    rpc = _reply(r.get("rid"), r.get("in") or {})
    return {"rpc": rpc, "written": STATE.get("written")}


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
