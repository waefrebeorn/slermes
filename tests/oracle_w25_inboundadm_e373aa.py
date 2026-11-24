#!/usr/bin/env python3
"""Differential oracle for gateway/relay/inbound_admission.py (gate w25_inboundadm_e373aa).

The live module is loaded from the quarry and its two entry points are driven
with the transport replaced by fixed inputs:

  * ``admit_relay_delivery`` is called for real; its lane dictionary is watched
    to learn the key the frame is ordered under (the admission coroutine itself
    is parked so the lane stays registered while we read it);
  * ``_admit`` is awaited for real with ``admit_producer`` stubbed to each
    fixture's outcome, so the compared value is which of its five exits ran:
    the parked-route refusal, the permanent-refusal ACK, the withheld ACK
    (re-raised), the consumer-reply delivery, or a quiet commit.

The permanent-refusal set is read live from ``gateway.relay.adapter``.
"""
import asyncio
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_inboundadm_e373aa"
MODULE = QUARRY / "gateway" / "relay" / "inbound_admission.py"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.core.errors import AuthorityRefusal  # noqa: E402

ADAPTER_MODULE = QUARRY / "gateway" / "relay" / "adapter.py"


def permanent_refusals():
    """The literal ``_PERMANENT_REFUSALS`` table, read from the live adapter source.

    Importing ``gateway.relay.adapter`` would drag the whole platform stack in
    for one frozen set; the assignment's own literal is the truth being ported,
    so it is evaluated statically instead.
    """
    import ast

    tree = ast.parse(ADAPTER_MODULE.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if not isinstance(node, ast.Assign):
            continue
        if not any(getattr(target, "id", None) == "_PERMANENT_REFUSALS" for target in node.targets):
            continue
        call = node.value
        if isinstance(call, ast.Call) and call.args:
            return set(ast.literal_eval(call.args[0]))
        return set(ast.literal_eval(call))
    raise AssertionError("_PERMANENT_REFUSALS not found in %s" % ADAPTER_MODULE)


PERMANENT = permanent_refusals()


class ParkedProfileRefusal(Exception):
    """Stands in for gateway.parked_route_refusal.ParkedProfileRefusal."""

    def __init__(self, profile):
        self.profile = profile
        super().__init__(profile)


FLAGS = {"refused": False, "delivered": False, "producer_calls": 0}
STATE = {}


def stub(name, **attrs):
    module = types.ModuleType(name)
    for key, value in attrs.items():
        setattr(module, key, value)
    sys.modules[name] = module
    return module


def refuse_parked_route(adapter, source, profile, reply_to=None):
    FLAGS["refused"] = True


async def admit_producer(adapter, event):
    FLAGS["producer_calls"] += 1
    kind = STATE.get("kind")
    if kind == "parked":
        raise ParkedProfileRefusal("profile-a")
    if kind == "authority":
        raise AuthorityRefusal(STATE.get("reason") or "")
    return STATE.get("receipt")


async def deliver_response(adapter, event, session_key, reply):
    FLAGS["delivered"] = True


gateway_pkg = types.ModuleType("gateway")
gateway_pkg.__path__ = []
sys.modules.setdefault("gateway", gateway_pkg)
# The permanent-refusal table is the seam this decision reads out of the relay
# adapter; the rest of that module (transports, adapters) is not involved.
stub("gateway.relay.adapter", _PERMANENT_REFUSALS=frozenset(PERMANENT))
stub("gateway.parked_route_refusal", ParkedProfileRefusal=ParkedProfileRefusal,
     refuse_parked_route=refuse_parked_route)
stub("gateway.platforms.webhook_ingress", admit_producer=admit_producer)
stub("gateway.session_ingress", deliver_response=deliver_response)


def load_live():
    spec = importlib.util.spec_from_file_location("live_inbound_admission", MODULE)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


live = load_live()

WANTED = {"lane_key", "admit_action", "constants"}


def make_event(source):
    platform = source.get("platform") or {}
    return types.SimpleNamespace(
        source=types.SimpleNamespace(
            platform=types.SimpleNamespace(value=platform.get("value")),
            chat_id=source.get("chat_id")),
        message_id="message-1")


async def lane_of(r):
    lanes = {}
    adapter = types.SimpleNamespace(_admission_lanes=lanes, _background_tasks=set())
    runner = types.SimpleNamespace(_session_key_for_source=lambda s: "session-key")
    event = make_event(r.get("source") or {})
    original = live._admit_after

    async def parked(previous, adapter_, runner_, event_):
        await asyncio.Event().wait()

    live._admit_after = parked
    try:
        task = await live.admit_relay_delivery(adapter, runner, event)
        keys = [list(key) for key in lanes.keys()]
        task.cancel()
        return keys[0]
    finally:
        live._admit_after = original


def admit_action(r):
    FLAGS["refused"] = False
    FLAGS["delivered"] = False
    STATE.clear()
    STATE.update(r)
    adapter = types.SimpleNamespace(_background_tasks=set())
    runner = types.SimpleNamespace(_session_key_for_source=lambda s: "session-key")
    event = types.SimpleNamespace(
        source=types.SimpleNamespace(platform=types.SimpleNamespace(value="relay"), chat_id="chat-1"),
        message_id="message-1",
        _consumer_reply=r.get("consumer_reply"))
    try:
        asyncio.run(live._admit(adapter, runner, event))
    except AuthorityRefusal:
        return "raise"
    if FLAGS["refused"]:
        return "refuse_parked"
    if FLAGS["delivered"]:
        return "deliver_consumer_reply"
    if r.get("kind") == "authority":
        return "ack_refusal"
    return "committed"


def constants():
    return {"permanent_refusals": sorted(PERMANENT)}


def expected(r):
    f = r.get("func")
    if f == "lane_key":
        return asyncio.run(lane_of(r))
    if f == "admit_action":
        return admit_action(r)
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
