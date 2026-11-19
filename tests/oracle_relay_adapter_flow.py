#!/usr/bin/env python3
"""Oracle for the gateway/relay/adapter.py relay FLOW cores
(relay_adapter_flow.c).

Drives the LIVE functions with SCRIPTED I/O: the functions are AST-extracted
from the quarry (no heavy gateway import), their module/class constants are
seeded (Assign AND AnnAssign; the real CapabilityDescriptor / SendResult
dataclasses; the LIVE egress decline classifiers), and every transport /
negotiation / redial / namespace / log dependency is replaced by a recording
monkeypatch stub returning the same scripted results the C harness feeds its
seams. Compares the DECISION SEQUENCE (ordered trace of seam calls) plus the
return value, not just a final scalar.

Input: stdin JSON lines from t_port_relay_adapter_flow:
    {"func": ..., "in": {...}, "out": {"ret": ..., "trace": [...], ...}}
Summary: ORACLE relay_adapter_flow: N cases, M mismatches. Exit 1 on any
mismatch, and fail-closed (exit 1) when zero cases were compared.
"""
import ast
import asyncio
import copy
import dataclasses
import json
import logging
import os
import re
import sys
import types
from typing import Any, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "relay", "adapter.py")

WANTED_MODULE = {
    "_utf16_len", "_send_result", "_event_ids", "_profile_from_session_key",
}
WANTED_METHODS = {
    # targets
    "_negotiated_descriptor", "_metrics_platform", "_slack_unfurl_hints",
    "prefers_fresh_final_streaming", "_gated_op", "_outbound",
    "_toggle_transport_redial", "_ack_emoji", "_stamp_format_hints",
    "_typing_frame",
    # live dependencies the targets compose (run REAL, not scripted)
    "_relay_platform_extra", "_relay_slack_extra", "_coerce_flag",
    "_format_hints", "_with_scope", "_with_status_thread_anchor",
}
ACK_CONSTS = {"_ACK_EMOJI_DEFAULT", "_ACK_EMOJI_BY_PLATFORM",
              "_ACK_PLATFORM_UNRESOLVED"}

CUR = {"func": None, "in": {}, "trace": []}


def rec(ev):
    CUR["trace"].append(ev)


def cin(key, default=None):
    return CUR["in"].get(key, default)


class RecLogger:
    def _log(self, level, fmt, *args):
        rec({"ev": "log", "level": level, "msg": fmt % args if args else fmt})

    def debug(self, fmt, *args, **kw):
        self._log("debug", fmt, *args)

    def info(self, fmt, *args, **kw):
        self._log("info", fmt, *args)

    def warning(self, fmt, *args, **kw):
        self._log("warning", fmt, *args)

    def error(self, fmt, *args, **kw):
        self._log("error", fmt, *args)

    def log(self, level, fmt, *args, **kw):
        self._log(level, fmt, *args)


def exec_node(node, ns, path):
    exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)


def extract_class(path, name, ns):
    tree = ast.parse(open(path, encoding="utf-8").read())
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == name:
            exec_node(node, ns, path)
            return True
    return False


def extract():
    src = open(SRC, encoding="utf-8").read()
    # adapter.py has `from __future__ import annotations` — signatures are
    # strings, but seed the names the module constants reference anyway.
    ns = {
        "__name__": "relay_adapter_extracted",
        "re": re, "logging": logging, "logger": RecLogger(),
        "Any": Any, "Optional": Optional,
        "Platform": types.SimpleNamespace(
            SLACK=types.SimpleNamespace(value="slack"),
            RELAY=types.SimpleNamespace(value="relay")),
        "dataclass": dataclasses.dataclass,
        "asdict": dataclasses.asdict,
        "json": json,
    }
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.Assign):
            if not all(isinstance(t, ast.Name) for t in node.targets):
                continue
            try:
                exec_node(node, ns, SRC)
            except Exception:
                pass
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            try:
                exec_node(node, ns, SRC)
            except Exception:
                pass
    # `logger = logging.getLogger(__name__)` is a module Assign — restore the
    # recording logger AFTER seeding so seam calls reach the trace.
    ns["logger"] = RecLogger()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED_MODULE:
            exec_node(node, ns, SRC)
        if isinstance(node, ast.ClassDef) and node.name == "RelayAdapter":
            for sub in node.body:
                if isinstance(sub, ast.Assign):
                    # class-level constants (_ACK_* vocabulary, caps)
                    if all(isinstance(t, ast.Name) and t.id in ACK_CONSTS for t in sub.targets):
                        exec_node(sub, ns, SRC)
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in WANTED_METHODS:
                    sub = copy.copy(sub)
                    sub.decorator_list = []   # staticmethod/classmethod wrappers
                    exec_node(sub, ns, SRC)
    # Real dataclasses from the LIVE sibling modules.
    base = os.path.join(QUARRY, "gateway", "platforms", "base.py")
    if not extract_class(base, "SendResult", ns):
        raise RuntimeError("SendResult class not found in quarry")
    desc = os.path.join(QUARRY, "gateway", "relay", "descriptor.py")
    if not extract_class(desc, "CapabilityDescriptor", ns):
        raise RuntimeError("CapabilityDescriptor class not found in quarry")
    # LIVE egress decline classifiers (their logger records into the trace).
    epath = os.path.join(QUARRY, "gateway", "relay", "egress.py")
    esrc = open(epath, encoding="utf-8").read()
    ens = {"__name__": "relay_egress_extracted", "logger": RecLogger(),
           "logging": logging, "Any": Any, "Dict": dict, "Optional": Optional,
           "Set": set}
    etree = ast.parse(esrc)
    for node in etree.body:
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            try:
                exec_node(node, ens, epath)
            except Exception:
                pass
    ens["logger"] = RecLogger()   # module Assign restored to the recorder
    for node in etree.body:
        if isinstance(node, ast.FunctionDef) and node.name in (
                "is_egress_decline", "decline_error", "log_decline"):
            exec_node(node, ens, epath)
    for k in ("is_egress_decline", "decline_error", "log_decline"):
        ns[k] = ens[k]
    return ns


def install_stubs(ns):
    # function-local `from gateway.session import profile_from_session_key_namespace`
    def ns_stub(namespace):
        r = cin("namespace_profile")
        rec({"ev": "resolve_namespace", "ns": namespace, "ret": r})
        return r

    gw = types.ModuleType("gateway")
    sess = types.ModuleType("gateway.session")
    sess.profile_from_session_key_namespace = ns_stub
    gw.session = sess
    sys.modules["gateway"] = gw
    sys.modules["gateway.session"] = sess


def make_descriptor(ns, dj):
    if dj is None:
        return None
    cd = ns["CapabilityDescriptor"]
    ops = dj.get("supported_ops")
    return cd(
        contract_version=dj.get("contract_version", 1),
        platform=dj.get("platform", ""),
        label=dj.get("label", "relay"),
        max_message_length=dj.get("max_message_length", 4096),
        supports_draft_streaming=dj.get("supports_draft_streaming", False),
        supports_edit=dj.get("supports_edit", True),
        supports_threads=dj.get("supports_threads", True),
        markdown_dialect=dj.get("markdown_dialect", "markdown"),
        len_unit=dj.get("len_unit", "chars"),
        supports_block_formatting=dj.get("supports_block_formatting", False),
        supports_inchannel_continuable=dj.get("supports_inchannel_continuable", False),
        supported_ops=tuple(ops) if ops else (),
    )


def make_transport(st):
    tj = st.get("transport")
    if tj is None:
        return None
    t = types.SimpleNamespace()

    async def send_stub(action, platform=None):
        if cin("send_raises", False):
            rec({"ev": "send_outbound", "action": action, "platform": platform,
                 "raised": True})
            raise RuntimeError("transport down")
        r = cin("send_ret")
        rec({"ev": "send_outbound", "action": action, "platform": platform, "ret": r})
        return r

    def resolve_stub(platform):
        if cin("resolve_raises", False):
            rec({"ev": "descriptor_for_platform", "platform": platform, "raised": True})
            raise RuntimeError("negotiation failed")
        r = cin("resolve_ret")
        rec({"ev": "descriptor_for_platform", "platform": platform, "ret": r})
        return r

    def make_toggle(name):
        def stub():
            if cin("toggle_raises", False):
                rec({"ev": "toggle", "method": name, "raised": True})
                raise RuntimeError("toggle failed")
            rec({"ev": "toggle", "method": name})
        return stub

    for k, v in tj.items():
        if k == "_identities":
            t._identities = [tuple(p) for p in (v or [])]
        elif isinstance(v, dict) and "callable" in v:
            if not v.get("callable"):
                setattr(t, k, 42)   # present but non-callable attribute
            elif k == "descriptor_for_platform":
                t.descriptor_for_platform = resolve_stub
            elif k in ("hold_redial", "release_redial"):
                setattr(t, k, make_toggle(k))
            else:
                setattr(t, k, make_toggle(k))
        else:
            setattr(t, k, v)
    if not hasattr(t, "send_outbound"):
        t.send_outbound = send_stub
    return t


def make_self(ns, inp):
    st = inp.get("state") or {}
    slf = types.SimpleNamespace()
    for k in ACK_CONSTS:
        setattr(slf, k, ns[k])
    for m in ("platform_by_chat", "chat_type_by_chat", "scope_by_chat",
              "dm_user_by_chat", "profile_by_chat", "last_inbound_ts_by_chat"):
        setattr(slf, "_" + m, dict(st.get(m) or {}))
    cfg = st.get("config")
    slf.config = types.SimpleNamespace(extra=cfg.get("extra")) if isinstance(cfg, dict) else None
    slf.descriptor = make_descriptor(ns, st.get("descriptor"))
    slf._transport = make_transport(st)
    # Composed live methods run REAL (extracted defs bound to the double).
    for m in ("_relay_platform_extra", "_relay_slack_extra", "_format_hints",
              "_with_scope", "_with_status_thread_anchor", "_outbound",
              "_slack_unfurl_hints"):
        setattr(slf, m, types.MethodType(ns[m], slf))
    slf._coerce_flag = ns["_coerce_flag"]   # staticmethod
    return slf


def make_event(ej):
    e = types.SimpleNamespace()
    if "message_id" in ej:
        e.message_id = ej["message_id"]
    if "source" in ej:
        sj = ej["source"]
        if sj is None:
            e.source = None
        else:
            src = types.SimpleNamespace()
            for k, v in sj.items():
                if k == "platform" and isinstance(v, dict) and "value" in v:
                    setattr(src, k, types.SimpleNamespace(value=v["value"]))
                else:
                    setattr(src, k, v)
            e.source = src
    return e


def norm_result(x):
    if isinstance(x, tuple):
        return [norm_result(v) for v in x]
    if isinstance(x, list):
        return [norm_result(v) for v in x]
    if isinstance(x, dict):
        return {k: norm_result(v) for k, v in x.items()}
    return x


def drive(ns, func, inp):
    if func == "utf16_len":
        return {"ret": ns["_utf16_len"](inp.get("text", ""))}

    if func == "send_result":
        r = ns["_send_result"](inp.get("result") or {}, **(inp.get("extra") or {}))
        return {"ret": norm_result(dataclasses.asdict(r))}

    if func == "event_ids":
        r = ns["_event_ids"](make_event(inp.get("event") or {}))
        return {"ret": norm_result(r)}

    if func == "profile_from_session_key":
        return {"ret": ns["_profile_from_session_key"](inp.get("session_key"))}

    if func == "negotiated_descriptor":
        slf = make_self(ns, inp)
        return {"ret": ns["_negotiated_descriptor"](slf, inp.get("platform"))}

    if func == "metrics_platform":
        slf = make_self(ns, inp)
        return {"ret": ns["_metrics_platform"](slf, inp.get("chat_id", ""))}

    if func == "slack_unfurl_hints":
        slf = make_self(ns, inp)
        return {"ret": ns["_slack_unfurl_hints"](slf, inp.get("platform"))}

    if func == "prefers_fresh_final_streaming":
        slf = make_self(ns, inp)
        r = ns["prefers_fresh_final_streaming"](
            slf, inp.get("content"), metadata=inp.get("metadata"),
            chat_id=inp.get("chat_id"))
        return {"ret": r}

    if func == "gated_op":
        slf = make_self(ns, inp)
        r = asyncio.run(ns["_gated_op"](
            slf, inp.get("chat_id", ""), inp.get("action") or {},
            decline_level=inp.get("decline_level"),
            subject=inp.get("subject"), platform=inp.get("platform"),
            surface_declines=inp.get("surface_declines", False),
            surface_ambiguous=inp.get("surface_ambiguous", False)))
        return {"ret": r}

    if func == "outbound":
        slf = make_self(ns, inp)
        try:
            r = asyncio.run(ns["_outbound"](slf, inp.get("chat_id", ""),
                                            inp.get("action") or {}))
            return {"ret": r, "raised": False}
        except Exception:
            return {"ret": None, "raised": True}

    if func == "toggle_transport_redial":
        slf = make_self(ns, inp)
        return {"ret": ns["_toggle_transport_redial"](slf, inp.get("method_name", "hold_redial"))}

    if func == "ack_emoji":
        slf = make_self(ns, inp)
        r = ns["_ack_emoji"](slf, make_event(inp.get("event") or {}), inp.get("chat_id", ""))
        return {"ret": norm_result(r)}

    if func == "stamp_format_hints":
        slf = make_self(ns, inp)
        desc = make_descriptor(ns, inp.get("descriptor"))
        r = ns["_stamp_format_hints"](slf, desc, inp.get("platform"), inp.get("metadata"))
        return {"ret": r}

    if func == "typing_frame":
        slf = make_self(ns, inp)
        asyncio.run(ns["_typing_frame"](slf, inp.get("chat_id", ""),
                                        inp.get("metadata"), inp.get("content"),
                                        inp.get("lane", "send_typing")))
        return {"ret": None}

    raise ValueError("unknown func: %s" % func)


def canon(x, ordered):
    if isinstance(x, dict):
        items = [(k, canon(v, ordered)) for k, v in x.items()]
        if not ordered:
            items = sorted(items, key=lambda kv: kv[0])
        return ("obj", items)
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v, ordered) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


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
        CUR["func"] = func
        CUR["in"] = inp
        CUR["trace"] = []
        try:
            want = drive(ns, func, inp)
        except Exception as e:
            mism += 1
            print("PYERR %s: %r" % (func, e))
            continue
        want["trace"] = CUR["trace"]
        # The typing frame's wire dict insertion order is part of the contract
        # (op, chat_id, metadata, content) — compare it ordered, trace included.
        ordered = func == "typing_frame"
        ok = canon(want, ordered) == canon(got, ordered)
        if not ok:
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE relay_adapter_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())