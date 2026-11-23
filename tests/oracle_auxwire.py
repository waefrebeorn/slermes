#!/usr/bin/env python3
"""Oracle for agent/auxiliary_wire.py (gate auxwire).

relay_chat_metadata is compared directly. prepare_chat_messages is driven with
a fake OpenAI SDK client type and stubbed shape_wire_carriers /
ChatCompletionsTransport that return the fixture's final wire messages; the C
side receives the same resolved facts and its rebuild is compared.
"""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def _pkg(name, path):
    m = types.ModuleType(name)
    m.__path__ = [path]
    sys.modules[name] = m


_pkg("agent", QUARRY + "/agent")
_pkg("agent.transports", QUARRY + "/agent/transports")


class _FakeSDK:
    pass


openai_stub = types.ModuleType("openai")
openai_stub.OpenAI = _FakeSDK
openai_stub.AsyncOpenAI = _FakeSDK
sys.modules["openai"] = openai_stub

_shaped = {"v": None}
rc = types.ModuleType("agent.reasoning_carriers")
rc.shape_wire_carriers = lambda messages, **kw: messages
sys.modules["agent.reasoning_carriers"] = rc

cc = types.ModuleType("agent.transports.chat_completions")


class _Transport:
    def convert_messages(self, messages, **kw):
        return _shaped["v"]


cc.ChatCompletionsTransport = _Transport
sys.modules["agent.transports.chat_completions"] = cc

spec = importlib.util.spec_from_file_location(
    "agent.auxiliary_wire", QUARRY + "/agent/auxiliary_wire.py")
live = importlib.util.module_from_spec(spec)
sys.modules["agent.auxiliary_wire"] = live
spec.loader.exec_module(live)

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
    if func == "relay":
        py = live.relay_chat_metadata(r.get("metadata") or {})
    elif func == "prepare":
        _shaped["v"] = r.get("shaped_messages")
        client = _FakeSDK() if r.get("client_is_chat_sdk") else object()
        kwargs = dict(r.get("kwargs") or {})
        py = live.prepare_chat_messages(client, kwargs)
    else:
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE auxwire: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
