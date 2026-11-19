#!/usr/bin/env python3
"""Oracle for the agent/bedrock_adapter.py FLOW + leaf port
(src/agent/bedrock_adapter_flow.c).

Drives the LIVE defs with SCRIPTED seams: the 18 ported functions plus their
pure siblings are AST-extracted from the quarry (no heavy agent import);
module constants are seeded from Assign AND AnnAssign nodes in two passes
(the second binds _REASONING_FILTERS / _ANTHROPIC_TO_CONVERSE, whose values
reference extracted defs). Every config / credential-chain / profile-lookup
dependency is a recording stub fed the same scripts the C harness feeds its
seams. Compares the DECISION SEQUENCE (ordered "trace" of {ev,args,ret} seam
events) plus the canonical return, key order included.

Representation contract (both sides, see the port header):
- None -> null; tuples -> arrays; bytes -> base64 ASCII string projection
  (_decode_redacted re-encodes canonically: "AB==" -> "AA==").
- _reasoning_filter's returned filter function projects as its name.
- strip_redacted_reasoning's same-object identity projects as "same".
- Uncaught exception -> {"__raise__": cls, "msg": str(exc)} (BaseException).
- str()/strip()/lower() operands are ASCII strings/null in fixtures;
  tool_calls ids are strings; list-typed carriers are lists.
- resolve_profile seam: map arn -> resolved; missing arn -> the arn itself
  (the live unresolved fallback), recorded on EVERY call (policy re-entry
  calls it twice for a resolved foreign ARN issuer).

Input: stdin JSON lines from t_port_bedrock_adapter_flow:
    {"func": ..., "in": {...}, "out": {"ret": ..., "trace": [...], ...}}
Summary: ORACLE bedrock_adapter_flow: N cases, M mismatches. Exit 1 on any
mismatch, and fail-closed (exit 1) when zero cases were compared.
"""
import __future__
import ast
import base64
import builtins
import json
import logging
import os
import re
import sys
import types
from contextlib import suppress
from pathlib import Path
from types import SimpleNamespace
from urllib.parse import urlparse

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stdin, "reconfigure"):
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "bedrock_adapter.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "bedrock_adapter.py")

NAME = "bedrock_adapter_flow"

PORTED = [
    "scoped_aws_session_kwargs", "resolve_bedrock_runtime_region",
    "bedrock_region_from_runtime_url", "parse_bedrock_openai_model_id",
    "_mantle_url_parts", "bedrock_guardrail_config", "bedrock_guardrail_headers",
    "anthropic_response_guardrail_intervened", "_parse_tool_args",
    "_decode_redacted", "_image_block_from_data_url", "_system_blocks",
    "_replay_ordered_blocks", "_reasoning_details_blocks", "_assistant_blocks",
    "_converse_replay_policy", "_reasoning_filter", "strip_redacted_reasoning",
]
# pure siblings the targets compose (run REAL, not scripted)
HELPERS = [
    "resolve_bedrock_region", "_safe_text", "_tool_use_block",
    "_redacted_reasoning_block", "_reasoning_text_block",
    "_without_redacted_reasoning", "_minted_outside_claude",
    "_anthropic_ordered_blocks", "_convert_content_to_converse",
    "_verbatim", "_unsigned_reasoning", "_without_reasoning",
]
WANTED = set(PORTED) | set(HELPERS)

logging.disable(logging.CRITICAL)

_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile(node):
    return compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
ASSIGN_CODES = []
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile(_node)
    elif isinstance(_node, ast.Assign):
        if all(isinstance(t, ast.Name) for t in _node.targets):
            ASSIGN_CODES.append(_compile(_node))
    elif isinstance(_node, ast.AnnAssign) and _node.value and isinstance(_node.target, ast.Name):
        ASSIGN_CODES.append(_compile(_node))
_missing = WANTED - set(DEF_CODE)
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)


class RecLogger:
    def __init__(self, trace):
        self.trace = trace

    def _log(self, level, fmt, *args):
        self.trace.append({"ev": "log", "args": {"level": level},
                           "ret": fmt % args if args else fmt})

    def debug(self, fmt, *args, **kw):
        self._log("debug", fmt, *args)

    def info(self, fmt, *args, **kw):
        self._log("info", fmt, *args)

    def warning(self, fmt, *args, **kw):
        self._log("warning", fmt, *args)

    def error(self, fmt, *args, **kw):
        self._log("error", fmt, *args)


_BASE_NS = {
    "__name__": "bedrock_adapter_flow_live",
    "__builtins__": builtins,
    "re": re, "os": os, "json": json, "base64": base64, "logging": logging,
    "suppress": suppress, "SimpleNamespace": SimpleNamespace, "urlparse": urlparse,
}
# py3.14 annotation/dataclass resolution wants a registered module.
_MOD = types.ModuleType("bedrock_adapter_flow_live")
_MOD.__dict__.update(_BASE_NS)
sys.modules.setdefault("bedrock_adapter_flow_live", _MOD)


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


ARGMAP = {
    "home_override": lambda a, k: {},
    "secret_scope": lambda a, k: {},
    "multiplex_active": lambda a, k: {},
    "load_config": lambda a, k: {},
    "botocore_region": lambda a, k: {"variable": a[0]},
}


def build_case(rec):
    inp = rec.get("in") or {}
    io = inp.get("io") or {}
    trace = []
    ns = dict(_BASE_NS)
    ns["logger"] = RecLogger(trace)
    # pass 1: constants the defs read at exec time
    for code in ASSIGN_CODES:
        try:
            exec(code, ns)
        except Exception:
            pass
    for nm, code in DEF_CODE.items():
        exec(code, ns)
    # pass 2: constants whose values reference the extracted defs
    for code in ASSIGN_CODES:
        try:
            exec(code, ns)
        except Exception:
            pass
    ns["logger"] = RecLogger(trace)  # module Assign restored to the recorder

    def recorder(name):
        def stub(*a, **kw):
            args = ARGMAP[name](a, kw)
            v = io.get(name)
            if isinstance(v, dict) and isinstance(v.get("__raise__"), str):
                marker = {"__raise__": v["__raise__"], "msg": v.get("msg", "")}
                trace.append({"ev": name, "args": args, "ret": marker})
                raise make_exc(marker)
            trace.append({"ev": name, "args": args, "ret": v})
            return v
        return stub

    def resolve_profile(arn):
        m = io.get("resolve_profile") or {}
        r = m.get(arn, arn)
        trace.append({"ev": "resolve_profile", "args": {"arn": arn}, "ret": r})
        return r

    ns["_resolve_inference_profile_model_id"] = resolve_profile

    hconst = types.ModuleType("hermes_constants")
    hconst.get_hermes_home_override = recorder("home_override")
    hconst.hermes_home_key = lambda: "home"
    agent = types.ModuleType("agent")
    agent.__path__ = []
    ss = types.ModuleType("agent.secret_scope")
    ss.current_secret_scope = recorder("secret_scope")
    ss.is_multiplex_active = recorder("multiplex_active")
    ss.get_secret = lambda k, d="": d
    agent.secret_scope = ss
    hc = types.ModuleType("hermes_cli")
    hc.__path__ = []
    hcc = types.ModuleType("hermes_cli.config")
    hcc.load_config_readonly = recorder("load_config")
    hc.config = hcc
    boto = types.ModuleType("botocore")
    boto.__path__ = []
    bs = types.ModuleType("botocore.session")
    bs.get_session = lambda: SimpleNamespace(
        get_config_variable=recorder("botocore_region"))
    boto.session = bs
    sys.modules.update({
        "hermes_constants": hconst,
        "agent": agent, "agent.secret_scope": ss,
        "hermes_cli": hc, "hermes_cli.config": hcc,
        "botocore": boto, "botocore.session": bs,
    })
    return ns, trace


def norm(v):
    if isinstance(v, (bytes, bytearray)):
        return base64.b64encode(bytes(v)).decode("ascii")
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items()}
    return v


FILTER_NAMES = ("_verbatim", "_without_reasoning", "_unsigned_reasoning")


def drive(rec, ns):
    func = rec["func"]
    inp = rec.get("in") or {}
    if func == "scoped_aws_session_kwargs":
        return {"ret": ns[func]()}
    if func == "resolve_bedrock_runtime_region":
        old = os.environ
        os.environ = dict(inp.get("env") or {})
        try:
            r = ns[func](inp.get("config"))
        finally:
            os.environ = old
        return {"ret": r}
    if func == "bedrock_region_from_runtime_url":
        return {"ret": ns[func](inp.get("base_url"))}
    if func == "parse_bedrock_openai_model_id":
        return {"ret": ns[func](inp.get("model_id"))}
    if func == "_mantle_url_parts":
        return {"ret": ns[func](inp.get("base_url"))}
    if func in ("bedrock_guardrail_config", "bedrock_guardrail_headers"):
        return {"ret": ns[func](inp.get("config"))}
    if func == "anthropic_response_guardrail_intervened":
        rj = inp.get("response")
        resp = SimpleNamespace(**rj) if isinstance(rj, dict) else rj
        return {"ret": ns[func](resp)}
    if func == "_parse_tool_args":
        return {"ret": ns[func](inp.get("args"))}
    if func == "_decode_redacted":
        return {"ret": ns[func](inp.get("encoded"))}
    if func == "_image_block_from_data_url":
        return {"ret": ns[func](inp.get("url", ""))}
    if func == "_system_blocks":
        return {"ret": ns[func](inp.get("content"))}
    if func == "_replay_ordered_blocks":
        return {"ret": ns[func](inp["blocks"])}
    if func == "_reasoning_details_blocks":
        return {"ret": ns[func](inp["msg"], inp.get("claude", False),
                                inp.get("own", False))}
    if func == "_assistant_blocks":
        return {"ret": ns[func](inp["msg"], inp.get("content"),
                                claude=inp.get("claude", True),
                                own=inp.get("own", False))}
    if func == "_converse_replay_policy":
        return {"ret": ns[func](inp.get("model"))}
    if func == "_reasoning_filter":
        f = ns[func](inp["policy"], inp.get("issuer"), inp.get("own", False),
                     in_loop=inp.get("in_loop", False))
        for nm in FILTER_NAMES:
            if f is ns[nm]:
                return {"ret": nm[1:]}
        raise ValueError("unmapped filter function")
    if func == "strip_redacted_reasoning":
        kw = inp["kwargs"]
        r = ns[func](kw)
        return {"ret": r, "same": r is kw}
    raise KeyError(f"unknown func {func}")


def canon(x):
    if isinstance(x, dict):
        return ("obj", [(k, canon(v)) for k, v in x.items()])
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def main():
    n = mism = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        try:
            ns, trace = build_case(rec)
            try:
                out = drive(rec, ns)
                out = {k: norm(v) for k, v in out.items()}
            except BaseException as e:  # noqa: BLE001 - fail-closed per case
                out = {"ret": {"__raise__": type(e).__name__, "msg": str(e)}}
            out["trace"] = trace
        except BaseException as e:  # noqa: BLE001 - seeding failure is a mismatch
            out = {"ret": {"__raise__": type(e).__name__, "msg": str(e)}, "trace": []}
        got = rec.get("out")
        if canon(out) != canon(got):
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C ={json.dumps(got, ensure_ascii=False)[:600]}")
                print(f"  PY={json.dumps(out, ensure_ascii=False)[:600]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
