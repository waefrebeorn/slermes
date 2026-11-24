#!/usr/bin/env python3
"""Differential oracle for gateway/relay/prompt_resolvers.py
(gate w22_promptreso_7bfabf).

The resolvers run live: every coroutine and plain function is lifted out of the
quarry source with an AST walk and executed against stand-in seam modules.

Stubbed seams (the live transport / pending stores, never re-implemented):
  gateway.relay.adapter          labels only -- the two dicts are excised from
                                 the real adapter.py, so the C tables are checked
                                 against the shipped text.
  gateway.approval_actor         the authority core: records (name, args,
                                 kwargs) and returns the fixture's requester.
  gateway.relay.admitted_authors answer_admitted -> fixture bool.
  tools.approval                 resolve_gateway_approval_card -> fixture count.
  tools.slash_confirm            async resolve -> fixture text.
  tools.clarify_gateway          mark_awaiting_text / resolve_gateway_clarify
                                 record the calls.

The compared value is the recorded trace, so argument placement, defaults
("deny"/"cancel"), the expired label and the clarify index mapping are all
checked against the live code.
"""
import ast
import asyncio
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "gateway" / "relay" / "prompt_resolvers.py"
ADAPTER = QUARRY / "gateway" / "relay" / "adapter.py"
GATE = "w22_promptreso_7bfabf"

WANTED = {"actor", "refusal_plan", "exec_ack", "slash_ack", "clarify"}


# --- live labels ------------------------------------------------------------
_labels = {}
_adapter_tree = ast.parse(ADAPTER.read_text(encoding="utf-8"))
for _node in _adapter_tree.body:
    if isinstance(_node, ast.Assign):
        names = {t.id for t in _node.targets if isinstance(t, ast.Name)}
        if names & {"_EXEC_APPROVAL_LABELS", "_SLASH_CONFIRM_LABELS"}:
            exec(compile(ast.Module(body=[_node], type_ignores=[]), str(ADAPTER), "exec"), _labels)
EXEC_LABELS = _labels["_EXEC_APPROVAL_LABELS"]
SLASH_LABELS = _labels["_SLASH_CONFIRM_LABELS"]


# --- the seam modules -------------------------------------------------------
CALLS = []
ACTS = []


def mk(name, **attrs):
    mod = types.ModuleType(name)
    for key, val in attrs.items():
        setattr(mod, key, val)
    sys.modules[name] = mod
    return mod


def _record(name):
    def fn(*args, **kwargs):
        CALLS.append({"fn": name, "args": list(args), "kwargs": dict(kwargs)})
        return REQUESTER
    return fn


REQUESTER = None

gateway_pkg = mk("gateway")
relay_pkg = mk("gateway.relay")
gateway_pkg.relay = relay_pkg

admitted_mod = mk("gateway.relay.admitted_authors",
                  answer_admitted=lambda event: ADMITTED)
relay_pkg.admitted_authors = admitted_mod

adapter_mod = mk("gateway.relay.adapter",
                 _EXEC_APPROVAL_LABELS=dict(EXEC_LABELS),
                 _SLASH_CONFIRM_LABELS=dict(SLASH_LABELS))
relay_pkg.adapter = adapter_mod

actor_mod = mk("gateway.approval_actor",
               approval_refusal=_record("approval_refusal"),
               slash_confirm_refusal=_record("slash_confirm_refusal"),
               clarify_refusal=_record("clarify_refusal"),
               refusal_text=lambda requester: requester,
               runner_answer_policy=lambda source: POLICY)
gateway_pkg.approval_actor = actor_mod

ADMITTED = True
POLICY = []

COUNT = 0


def _resolve_card(session_key, choice, request_id, **kwargs):
    CALLS.append({"fn": "resolve_gateway_approval_card",
                  "args": [session_key, choice, request_id], "kwargs": dict(kwargs)})
    return COUNT


approval_tools = mk("tools.approval", resolve_gateway_approval_card=_resolve_card)

RESULT_TEXT = None


async def _slash_resolve(session_key, confirm_id, choice):
    CALLS.append({"fn": "slash_confirm.resolve", "args": [session_key, confirm_id, choice],
                  "kwargs": {}})
    return RESULT_TEXT


slash_tools = mk("tools.slash_confirm", resolve=_slash_resolve)

CLARIFY = []


def _mark_awaiting(clarify_id):
    CLARIFY.append({"fn": "mark_awaiting_text", "args": [clarify_id]})


def _resolve_clarify(clarify_id, answer):
    CLARIFY.append({"fn": "resolve_gateway_clarify", "args": [clarify_id, answer]})


clarify_tools = mk("tools.clarify_gateway", mark_awaiting_text=_mark_awaiting,
                   resolve_gateway_clarify=_resolve_clarify)

tools_pkg = mk("tools")
tools_pkg.approval = approval_tools
tools_pkg.slash_confirm = slash_tools
tools_pkg.clarify_gateway = clarify_tools

# --- the live module --------------------------------------------------------
G = {"Optional": object}
_tree = ast.parse(SRC.read_text(encoding="utf-8"))
_wanted_fns = {"prompt_actor", "prompt_refusal", "resolve_exec_approval",
               "resolve_slash_confirm", "resolve_clarify"}
_found = set()
for _node in _tree.body:
    if isinstance(_node, (ast.FunctionDef, ast.AsyncFunctionDef)) and _node.name in _wanted_fns:
        _node.decorator_list = []
        exec(compile(ast.Module(body=[_node], type_ignores=[]), str(SRC), "exec"), G)
        _found.add(_node.name)
if _found != _wanted_fns:  # pragma: no cover
    raise SystemExit("prompt_resolvers source changed; missing %r" % sorted(_wanted_fns - _found))


class FakeAdapter:
    def __init__(self):
        self.canon = 0
        self.acks = []
        self.resumed = []

    def _canonicalize(self, source):
        self.canon += 1

    def _send_lifecycle_ack(self, chat_id, text, meta):
        self.acks.append(text)

    def resume_typing_for_chat(self, chat_id):
        self.resumed.append(chat_id)


class FakeSource:
    def __init__(self, user_id, missing):
        if not missing:
            self.user_id = user_id


class FakeEvent:
    def __init__(self, source):
        self.source = source


def actor(source, admin_ids, admitted):
    global POLICY, ADMITTED
    POLICY = admin_ids
    ADMITTED = bool(admitted)
    adapter = FakeAdapter()
    missing = "user_id" not in source
    src = FakeSource(source.get("user_id"), missing)
    got = G["prompt_actor"](adapter, FakeEvent(src))
    return dict(got)


def refusal_plan(state, actor_dict, option_id, requester):
    global REQUESTER
    REQUESTER = requester
    CALLS.clear()
    result = G["prompt_refusal"](state, actor_dict, option_id)
    call = CALLS[0]
    return {"fn": call["fn"], "args": call["args"], "kwargs": call["kwargs"],
            "refused": result is not None}


def exec_ack(option_id, count, actor_state):
    global COUNT
    COUNT = count
    CALLS.clear()
    adapter = FakeAdapter()
    state = {"session_key": "sess-1", "request_id": "req-1"}
    if actor_state is not None:
        state["actor"] = actor_state
    asyncio.run(G["resolve_exec_approval"](adapter, state, option_id, "chat-1", {"m": 1}))
    settle = CALLS[0]
    return {"choice": settle["args"][1],
            "label": adapter.acks[0] if adapter.acks else None,
            "acks": list(adapter.acks),
            "resume": bool(adapter.resumed),
            "kwargs": settle["kwargs"]}


def slash_ack(option_id, result_text):
    global RESULT_TEXT
    RESULT_TEXT = result_text
    CALLS.clear()
    adapter = FakeAdapter()
    asyncio.run(G["resolve_slash_confirm"](adapter, {"session_key": "sess-1", "confirm_id": "cf-1"},
                                          option_id, "chat-1", {}))
    return {"choice": CALLS[0]["args"][2], "acks": list(adapter.acks)}


def clarify(option_id, choices):
    CLARIFY.clear()
    adapter = FakeAdapter()
    asyncio.run(G["resolve_clarify"](adapter, {"clarify_id": "cl-1", "choices": choices},
                                    option_id, "chat-1", {}))
    resolved = [c for c in CLARIFY if c["fn"] == "resolve_gateway_clarify"]
    if resolved:
        action = "resolve"
    elif adapter.acks:
        action = "await_text"
    else:
        action = "fallback_text"
    return {"action": action,
            "ack": adapter.acks[0] if adapter.acks else None,
            "value": resolved[0]["args"][1] if resolved else None}


def expected(r):
    f = r.get("func")
    if f == "actor":
        return actor(r.get("source") or {}, r.get("admin_ids") or [], r.get("admitted", True))
    if f == "refusal_plan":
        return refusal_plan(r.get("state") or {}, r.get("actor") or {},
                            r.get("option_id"), r.get("requester"))
    if f == "exec_ack":
        return exec_ack(r.get("option_id"), int(r.get("count") or 0), r.get("actor"))
    if f == "slash_ack":
        return slash_ack(r.get("option_id"), r.get("result_text"))
    if f == "clarify":
        return clarify(r.get("option_id"), r.get("choices") or [])
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
