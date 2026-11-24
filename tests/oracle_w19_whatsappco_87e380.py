#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/whatsapp_common.py (gate w19_whatsappco_87e380).

Execs the live AST `_stash` / `_header_to_bold` helpers and the WhatsAppBehaviorMixin
(with a stub base and the real `_decode_json_list_literal`) into stub-filled globals.
Config extras and env carriers come from the fixture; the class methods themselves are
the live ones.
"""
import ast
import json
import logging
import re
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
SRC = Q + "/gateway/platforms/whatsapp_common.py"
SHARED = Q + "/gateway/platforms/_shared.py"
GATE = "w19_whatsappco_87e380"
_ANN = __import__("__future__").annotations.compiler_flag

ENV = {}


def _exec_node(node, g, path):
    mod = ast.Module(body=[node], type_ignores=[])
    exec(compile(mod, path, "exec", flags=_ANN, dont_inherit=True), g)


def get_secret(name, default=None):
    v = ENV.get(name, None)
    return default if v is None else v


# == _decode_json_list_literal, straight out of _shared.py ===================
sg = {"__name__": "wa_shared", "json": json}
stree = ast.parse(open(SHARED, encoding="utf-8").read())
for node in stree.body:
    if isinstance(node, ast.FunctionDef) and node.name == "decode_json_list_literal":
        _exec_node(node, sg, SHARED)

_live = types.ModuleType("whatsapp_live")
_live.__package__ = "gateway.platforms"
sys.modules["whatsapp_live"] = _live
g = _live.__dict__


class OwnAccessPolicyMixin:
    pass


g.update({"re": re, "json": json, "logging": logging, "logging_module": logging,
          "logger": logging.getLogger("oracle"), "Optional": object, "Any": object,
          "Dict": object, "Path": object, "_get_wsecret": get_secret,
          "_extra_or_wsecret": lambda extra, key, env: None,
          "_decode_json_list_literal": sg["decode_json_list_literal"],
          "OwnAccessPolicyMixin": OwnAccessPolicyMixin})

tree = ast.parse(open(SRC, encoding="utf-8").read())
for node in tree.body:
    if isinstance(node, ast.FunctionDef) and node.name in ("_stash", "_header_to_bold"):
        _exec_node(node, g, SRC)
    elif isinstance(node, ast.ClassDef) and node.name == "WhatsAppBehaviorMixin":
        _exec_node(node, g, SRC)

Mixin = g["WhatsAppBehaviorMixin"]


def _inst(**attrs):
    o = object.__new__(Mixin)
    o.config = SimpleNamespace(extra=attrs.get("extra") or {})
    o.name = "wa"
    o._reply_prefix = attrs.get("reply_prefix")
    o._mention_patterns = []
    o._allow_from = set()
    o._dm_allowlist_source = attrs.get("dm_source")
    return o


def run_case(r):
    f = r.get("func")
    if f == "header_to_bold":
        return g["_header_to_bold"](SimpleNamespace(group=lambda _n, _v=r.get("inner", ""): _v))
    if f == "sanitize_outbound_text":
        return Mixin._sanitize_outbound_text(r.get("content", ""))
    if f == "effective_reply_prefix":
        ENV.clear()
        if r.get("mode") is not None:
            ENV["WHATSAPP_MODE"] = r.get("mode")
        if r.get("env_prefix") is not None:
            ENV["WHATSAPP_REPLY_PREFIX"] = r.get("env_prefix")
        o = _inst(reply_prefix=r.get("reply_prefix"))
        return o._effective_reply_prefix()
    if f == "outgoing_chunk_limit":
        o = _inst()
        n = int(r.get("prefix_len") or 0)
        o._effective_reply_prefix = lambda: "x" * n
        return o._outgoing_chunk_limit()
    if f == "normalize_whatsapp_id":
        return Mixin._normalize_whatsapp_id(r.get("value"))
    if f == "is_broadcast_chat":
        return bool(Mixin._is_broadcast_chat(r.get("chat_id") or ""))
    if f == "coerce_allow_list":
        return sorted(Mixin._coerce_allow_list(r.get("raw")))
    if f == "select_allowlist":
        extra = r.get("extra") or {}
        env = r.get("env") or {}
        source, raw = Mixin._select_allowlist(extra, tuple(r.get("config_keys") or []),
                                              tuple(r.get("env_keys") or []),
                                              lambda k: env.get(k))
        return {"source": source, "raw": raw}
    if f == "bot_ids_from_message":
        return sorted(_inst()._bot_ids_from_message(r.get("data") or {}))
    if f == "message_is_reply_to_bot":
        return bool(_inst()._message_is_reply_to_bot(r.get("data") or {}))
    if f == "message_mentions_bot":
        return bool(_inst()._message_mentions_bot(r.get("data") or {}))
    if f == "clean_bot_mention_text":
        return _inst()._clean_bot_mention_text(r.get("text") or "", r.get("data") or {})
    if f == "format_message":
        return _inst().format_message(r.get("content", ""))
    return "SKIP"


FUNCS = {"header_to_bold", "sanitize_outbound_text", "effective_reply_prefix",
         "outgoing_chunk_limit", "normalize_whatsapp_id", "is_broadcast_chat",
         "coerce_allow_list", "select_allowlist", "bot_ids_from_message",
         "message_is_reply_to_bot", "message_mentions_bot", "clean_bot_mention_text",
         "format_message"}


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
        f = r.get("func")
        if f not in FUNCS:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as exc:  # noqa: BLE001 - report, never abort the oracle
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (f, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
