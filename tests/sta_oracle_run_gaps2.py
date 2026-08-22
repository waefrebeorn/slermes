#!/usr/bin/env python3
"""sta_oracle_run_gaps2.py — live-Python twin for the run_gaps2 oracle.

gateway/run.py's full import chain is currently broken upstream
(hermes_constants.hermes_home_key missing), so per the unimportable-module
pattern we AST-extract the REAL source of the functions under test (plus
their module-level deps) and exec them — no reimplementation.
"""
import ast
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PY_RUN = os.path.join(ROOT, "gateway", "run.py")

SRC = open(PY_RUN, encoding="utf-8").read()
TREE = ast.parse(SRC)

# Functions / staticmethods under test + their pure dependencies.
WANT_FUNCS = {
    "_float_env",
    "build_resume_recovery_note",
    "_prepare_resume_pending_message",
    "_shorten_command_for_display",
    "_format_concise_process_notification",
    "_reconnect_needs_attention",
    "_redact_gateway_user_facing_secrets",
}
# Module-level constants needed by the above.
WANT_CONSTS = {"_RECONNECT_ATTENTION_AFTER_SECONDS", "_GATEWAY_SECRET_PATTERNS"}

ns = {
    # typing / stdlib names the extracted sources reference.
    "Optional": object,  # annotations are strings at runtime? no — provide real
}
from typing import Optional, Dict, List, Tuple, Any, Callable
ns["Optional"] = Optional; ns["Dict"]=Dict; ns["List"]=List; ns["Tuple"]=Tuple
ns["Any"]=Any; ns["Callable"]=Callable
import re as _re, time as _time, os as _os
ns.update({"re": _re, "time": _time, "os": _os, "dataclasses": __import__("dataclasses")})
import asyncio as _asyncio
ns["asyncio"] = _asyncio
for node in TREE.body:
    if isinstance(node, ast.FunctionDef) and node.name in WANT_FUNCS:
        code = ast.get_source_segment(SRC, node)
        exec(compile(ast.Module(body=[ast.parse(code).body[0]], type_ignores=[]),
                     PY_RUN, "exec"), ns)
    elif isinstance(node, ast.Assign):
        for t in node.targets:
            if isinstance(t, ast.Name) and t.id in WANT_CONSTS:
                code = ast.get_source_segment(SRC, node)
                exec(compile(ast.Module(body=[node], type_ignores=[]), PY_RUN, "exec"), ns)

# Staticmethods on GatewayRunner: extract the method defs directly and bind
# them as plain functions (they are @staticmethod — no self).
CLASS_WANTED = {
    "_completion_notification_batch_key",
    "_async_delegation_group_key",
    "_format_coalesced_process_completions",
    "_format_coalesced_async_delegations",
    "_final_text_for_post_turn_hooks",
}
for node in TREE.body:
    if isinstance(node, ast.ClassDef) and node.name == "GatewayRunner":
        for sub in node.body:
            if isinstance(sub, ast.FunctionDef) and sub.name in CLASS_WANTED:
                code = ast.get_source_segment(SRC, sub)
                tree = ast.parse(code)
                fn = tree.body[0]
                # strip decorators (@staticmethod) and any leading self-less args stay
                fn.decorator_list = []
                exec(compile(ast.Module(body=[fn], type_ignores=[]), PY_RUN, "exec"), ns)

GR = type("GatewayRunner", (), {})  # bare holder so staticmethods are callable
for name in CLASS_WANTED:
    setattr(GatewayRunner_holder := GR, name, staticmethod(ns[name]))

ns["_RECONNECT"] = ns.get("_RECONNECT_ATTENTION_AFTER_SECONDS")


def main():
    fx = json.load(open(sys.argv[1]))
    for op in fx.get("ops", []):
        k = op["kind"]
        if k == "shorten":
            out = ns["_shorten_command_for_display"](op["command"], op.get("limit", 80))
            print(json.dumps({"kind": "shorten", "out": out}))
        elif k == "concise":
            out = ns["_format_concise_process_notification"](
                op.get("session_id", ""), op.get("command", ""),
                op.get("exit_code"), op.get("output", ""), op.get("duration"))
            print(json.dumps({"kind": "concise", "out": out}))
        elif k == "batch_key":
            key = GR._completion_notification_batch_key(op["evt"])
            print(json.dumps({"kind": "batch_key", "key": "|".join(key)}))
        elif k == "adel_key":
            key = GR._async_delegation_group_key(op["evt"])
            print(json.dumps({"kind": "adel_key", "key": "|".join(key)}))
        elif k == "coalesce":
            entries = [(t[0], t[1], None) for t in op["entries"]]
            text = GR._format_coalesced_process_completions(entries)
            print(json.dumps({"kind": "coalesce", "text": text}))
        elif k == "coalesce_adel":
            text = GR._format_coalesced_async_delegations(op["blocks"])
            print(json.dumps({"kind": "coalesce_adel", "text": text}))
        elif k == "final_text":
            ar = op.get("agent_result")
            ev = op.get("event")
            ev_obj = None
            if ev is not None:
                class E:
                    pass
                ev_obj = E()
                setattr(ev_obj, "_streamed_final_response", ev.get("_streamed_final_response"))
            text = GR._final_text_for_post_turn_hooks(ar, ev_obj)
            print(json.dumps({"kind": "final_text", "text": text}))
        elif k == "reconnect":
            import os
            os.environ.pop("HERMES_RECONNECT_ATTENTION_AFTER_SECONDS", None)
            info = dict(op["info"])
            needs = ns["_reconnect_needs_attention"](info, op["now"])
            print(json.dumps({"kind": "reconnect", "needs": bool(needs),
                              "queued_at": info.get("queued_at")}))
        elif k == "resume_msg":
            rec, persist = ns["_prepare_resume_pending_message"](
                op.get("reason"), op.get("message"),
                interactive=op.get("interactive", True))
            print(json.dumps({"kind": "resume_msg", "recovery": rec,
                              "persist": persist}))


if __name__ == "__main__":
    main()
