#!/usr/bin/env python3
"""Oracle for openai_routes_flow.c — gateway/platforms/api_server_openai_routes.py
route FLOW cores + leaves (LIVE authority: the quarry module).

Drives the LIVE functions with SCRIPTED I/O: the 18 targets are AST-extracted
from the quarry (no heavy gateway/aiohttp import), module constants seeded
(Assign AND AnnAssign, plain-Name targets only), and every api_server helper
import, adapter route method, response-store read/write and turn-boundary
probe replaced by a recording stub returning the SAME scripted results the C
harness feeds its universal seam. Internal composition runs REAL on both
sides: _is_reasoning_input_item inside the parse chains, _cap_text inside the
cappers, _cap_history_tool_outputs inside _build_response_conversation_history
(which reaches the turn-boundary leaf through the REAL extracted
_response_messages_turn_start_index staticmethod), and json.loads/json.dumps
inside _cap_history_tool_outputs. Compares the ordered DECISION SEQUENCE +
return value, with ordered-dict comparison (insertion order is semantic for
the envelope builders). Per-case BaseException catch (SystemExit included)
compared as a raise spec.

The four route handlers (_handle_chat_completions, _handle_responses,
_handle_get_response, _handle_delete_response) are already ported elsewhere
and are NOT re-driven here.

Input: stdin JSON lines from t_port_openai_routes_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...]}}
Summary: ORACLE openai_routes_flow: N cases, M mismatches. Fail-closed
(exit 2) when extraction misses defs or zero cases were compared.

Fixture domain notes (kept identical on both sides):
- str()-coerced values (budget reason, history role) use int literals or
  non-integral floats (parsed JSON loses int/float identity);
- tool-call arguments blobs use standard JSON with integral numbers only
  (json.dumps float repr parity is out of domain), no raw control chars,
  invalid escapes, leading-zero numbers or NaN;
- normalize_* scripts return strings (or null to exercise the None-concat
  raise path); scripted raises never use KeyError (str(KeyError) reprs its
  argument);
- snapshot is null or a 2-item array (a genuine `a, b = snapshot` unpack);
  replay_record is null or a 2-item array (it is splatted into store.put(),
  so other lengths would raise a double-signature-dependent TypeError).
"""
import ast
import builtins
import json
import os
import sys
import types

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "platforms", "api_server_openai_routes.py")

PY = {
    "result_flags": "_result_flags",
    "budget_exhausted": "_budget_exhausted",
    "finish_reason": "_finish_reason",
    "settled": "_settled",
    "responses_fingerprint_keys": "_responses_fingerprint_keys",
    "response_status": "_response_status",
    "hermes_extras": "_hermes_extras",
    "post_stream_transform": "_post_stream_transform",
    "cap_text": "_cap_text",
    "is_reasoning_input_item": "_is_reasoning_input_item",
    "matching_replay": "_matching_replay",
    "store_nonstream_response": "_store_nonstream_response",
    "parse_chat_messages": "_parse_chat_messages",
    "parse_responses_input": "_parse_responses_input",
    "parse_conversation_history": "_parse_conversation_history",
    "select_request_route": "_select_request_route",
    "cap_history_tool_outputs": "_cap_history_tool_outputs",
    "build_response_conversation_history": "_build_response_conversation_history",
}
WANTED_FUNCS = {
    "_result_flags", "_budget_exhausted", "_finish_reason", "_settled",
    "_responses_fingerprint_keys", "_response_status", "_hermes_extras",
    "_post_stream_transform", "_cap_text", "_is_reasoning_input_item",
    "_matching_replay", "_store_nonstream_response", "_parse_chat_messages",
    "_parse_responses_input", "_parse_conversation_history",
    "_cap_history_tool_outputs",
}
# Mixin members: the transcript builder (static), the turn-boundary delegate
# (static, real inside the builder chain) and the route resolver (instance).
WANTED_STATIC = {"_build_response_conversation_history", "_response_messages_turn_start_index"}
WANTED_METHODS = {"_select_request_route"}

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}}


def _exc_class(name):
    cls = getattr(builtins, name, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls
    return type(name, (Exception,), {})


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        raise _exc_class(r["__raise__"])(r.get("msg", ""))
    return r


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    sc = CUR["scripts"].get(name)
    if sc is None:
        r = None
    elif isinstance(sc, list):
        i = CUR["idx"].get(name, 0)
        CUR["idx"][name] = i + 1
        r = sc[min(i, len(sc) - 1)] if sc else None
    else:
        r = sc
    # Deep-copy so live mutation of a result cannot corrupt the script/trace.
    r = json.loads(json.dumps(r))
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


def norm(o):
    if o is None or isinstance(o, (bool, int, float, str)):
        return o
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    """Ordered, type-tagged canonical form: dict insertion order is semantic."""
    o = _numnorm(o)
    if isinstance(o, dict):
        return ("obj", [(k, canon(v)) for k, v in o.items()])
    if isinstance(o, (list, tuple)):
        return ("arr", [canon(v) for v in o])
    if o is None:
        return ("null",)
    if isinstance(o, bool):
        return ("bool", o)
    if isinstance(o, (int, float)):
        return ("num", float(o))
    return ("str", o)


# ── extraction ──

def extract():
    import copy
    import logging
    import re
    import time
    import uuid
    from contextlib import suppress
    from typing import Any, Dict, List, Optional

    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("openai_routes_extracted")
    sys.modules["openai_routes_extracted"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "openai_routes_extracted",
        "json": json, "os": os, "sys": sys, "re": re, "copy": copy,
        "time": time, "uuid": uuid, "logging": logging, "suppress": suppress,
        "Any": Any, "Dict": Dict, "List": List, "Optional": Optional,
        "t": lambda key: key,
        "web": types.SimpleNamespace(Response=object, Request=object,
                                     StreamResponse=object,
                                     json_response=lambda *a, **k: None),
        "asyncio": types.SimpleNamespace(Lock=object, Task=object,
                                         QueueEmpty=type("QueueEmpty", (Exception,), {})),
    })
    tree = ast.parse(src)
    # Pass 1: module constants (Assign AND AnnAssign, plain-Name targets only).
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
        elif isinstance(node, ast.AnnAssign):
            if not (node.value and isinstance(node.target, ast.Name)):
                continue
        else:
            continue
        try:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
        except Exception:
            pass
    # Pass 2: the target module-level defs. future-annotations keeps
    # annotations strings so def-exec never evaluates web.Request etc.
    import __future__
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED_FUNCS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                         flags=__future__.annotations.compiler_flag), ns)
    # Pass 3: the mixin members (decorators stripped; statics call as plain
    # functions, the instance method takes the self double first).
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "OpenAICompatRoutesMixin":
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef) and \
                        sub.name in (WANTED_STATIC | WANTED_METHODS):
                    sub.decorator_list = []
                    exec(compile(ast.Module(body=[sub], type_ignores=[]), SRC, "exec",
                                 flags=__future__.annotations.compiler_flag), ns)
    missing = (WANTED_FUNCS | WANTED_STATIC | WANTED_METHODS) - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    if "_RESPONSES_FINGERPRINT_KEYS" not in ns:
        print("FATAL missing constant: _RESPONSES_FINGERPRINT_KEYS")
        sys.exit(2)

    class AdapterDouble:
        pass
    AdapterDouble._response_messages_turn_start_index = staticmethod(
        ns["_response_messages_turn_start_index"])
    ns["AdapterDouble"] = AdapterDouble
    return ns


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return m


def _register(name, mod):
    parts = name.split(".")
    parent = _pkg(".".join(parts[:-1]))
    setattr(parent, parts[-1], mod)
    sys.modules[name] = mod


def install_stubs(ns):
    """The lazy `from gateway.platforms.api_server import ...` lookups inside
    the live defs resolve to these recording stubs (same scripts as C)."""
    _pkg("gateway")
    _pkg("gateway.platforms")
    api = types.ModuleType("gateway.platforms.api_server")
    api._normalize_chat_content = lambda content: seam(
        "normalize_chat_content", {"content": content})
    api._normalize_multimodal_content = lambda content: seam(
        "normalize_multimodal_content", {"content": content})

    def _mve(exc, *, param):
        return seam("multimodal_validation_error", {"exc": str(exc), "param": param})
    api._multimodal_validation_error = _mve

    def _er(message, status, *, err_type="invalid_request_error", param=None,
            code=None, headers=None):
        return seam("error_response", {"message": message, "status": status})
    api._error_response = _er

    def _rao(body, *, virtual_model=None, allow_bare_model=True):
        return seam("request_agent_overrides",
                    {"body": body, "virtual_model": virtual_model,
                     "allow_bare_model": allow_bare_model})
    api._request_agent_overrides = _rao
    api.APIServerAdapter = ns["AdapterDouble"]
    _register("gateway.platforms.api_server", api)

    tb = types.ModuleType("gateway.platforms.api_server_turn_boundary")
    tb.response_turn_start_index = lambda ch, um, result: seam(
        "turn_start_index",
        {"conversation_history": ch, "user_message": um, "result": result})
    _register("gateway.platforms.api_server_turn_boundary", tb)


class StoreDouble:
    """Response-store double: every method is one scripted seam call."""

    def request_key_matches(self, key, binding):
        return seam("store.request_key_matches", {"key": key, "binding": binding})

    def get(self, key):
        return seam("store.get", {"key": key})

    def get_conversation(self, conversation):
        return seam("store.get_conversation", {"conversation": conversation})

    def put(self, key, value):
        return seam("store.put", {"key": key, "value": value})

    def set_conversation(self, conversation, response_id):
        return seam("store.set_conversation",
                    {"conversation": conversation, "response_id": response_id})


# ── drive ──

def drive(ns, func, inp):
    """Returns ret; ret is a raise spec on ANY BaseException."""
    name = PY[func]
    try:
        if func == "result_flags":
            ret = ns[name](inp.get("result"))
        elif func == "budget_exhausted":
            ret = ns[name](inp.get("result"))
        elif func == "finish_reason":
            ret = ns[name](inp.get("completed"), inp.get("is_partial"),
                           inp.get("is_failed"), inp.get("err_msg"),
                           inp.get("agent_error"),
                           budget_exhausted=inp.get("budget_exhausted", False))
        elif func == "settled":
            ret = ns[name](inp.get("record"))
        elif func == "responses_fingerprint_keys":
            ret = ns[name](inp.get("body"))
        elif func == "response_status":
            ret = ns[name](inp.get("result"))
        elif func == "hermes_extras":
            ret = ns[name](inp.get("completed"), inp.get("is_partial"),
                           inp.get("is_failed"), inp.get("err_msg"),
                           inp.get("finish_reason"),
                           budget_exhausted=inp.get("budget_exhausted", False))
        elif func == "post_stream_transform":
            ret = ns[name](inp.get("result"))
        elif func == "cap_text":
            ret = ns[name](inp.get("text"), inp.get("keep"))
        elif func == "is_reasoning_input_item":
            ret = ns[name](inp.get("item"))
        elif func == "matching_replay":
            ret = ns[name](StoreDouble(), inp.get("key"), inp.get("binding"))
        elif func == "store_nonstream_response":
            ret = ns[name](StoreDouble(), inp.get("snapshot"),
                           inp.get("replay_record"), inp.get("conversation"),
                           inp.get("terminal_replay", False))
        elif func == "parse_chat_messages":
            ret = ns[name](inp.get("messages"))
        elif func == "parse_responses_input":
            ret = ns[name](inp.get("input"))
        elif func == "parse_conversation_history":
            ret = ns[name](inp.get("history"))
        elif func == "select_request_route":
            def _rrce(*, session_id, gateway_session_key, requested_model,
                      requested_provider, route):
                return seam("request_route_conflict_error",
                            {"session_id": session_id,
                             "gateway_session_key": gateway_session_key,
                             "requested_model": requested_model,
                             "requested_provider": requested_provider,
                             "route": route})
            sd = types.SimpleNamespace(
                _model_name=inp.get("model_name"),
                _direct_model_requests=inp.get("direct_model_requests", True),
                _resolve_route=lambda model_alias: seam(
                    "resolve_route", {"model_alias": model_alias}),
                _request_route_conflict_error=_rrce)
            kw_gsk = inp.get("gateway_session_key")
            ret = ns[name](sd, inp.get("body"), session_id=inp.get("session_id"),
                           gateway_session_key=kw_gsk,
                           model_alias=inp.get("model_alias"))
        elif func == "cap_history_tool_outputs":
            ret = ns[name](inp.get("history"), inp.get("max_chars", 0))
        elif func == "build_response_conversation_history":
            ret = ns[name](inp.get("conversation_history"), inp.get("user_message"),
                           inp.get("result"), inp.get("final_response"),
                           tool_output_max_chars=inp.get("tool_output_max_chars", 0))
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:  # SystemExit included — fail-closed per case
        ret = {"__raise__": type(e).__name__, "msg": str(e)}
    return ret


def main():
    ns = extract()
    install_stubs(ns)
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        if func not in PY:
            mism += 1
            print("PYERR unknown func: %s" % func)
            continue
        ret = drive(ns, func, inp)
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE openai_routes_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE openai_routes_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
