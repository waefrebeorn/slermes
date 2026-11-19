#!/usr/bin/env python3
"""Oracle for the Gemini native transport UNCLAIMED-remainder FLOW port
(src/agent/transports/gemini_transport_flow.c).

The task names agent/transports/gemini.py; in this quarry snapshot the Gemini
transport IS agent/gemini_native_adapter.py ("OpenAI-compatible facade over
Google AI Studio's native Gemini API"), so live defs are AST-extracted from
there and the PoP lines credit that module.

Drives the LIVE defs with SCRIPTED seams: every collaborator (claimed siblings
is_vertex_express_base_url / gemini_http_error / _effective_gemini_max_output_tokens,
foreign-module imports hermes_cli.version_info.hermes_user_agent and
agent.gemini_schema.prepare_gemini_tool_parameters / sanitize_gemini_tool_parameters,
the nondeterministic uuid.uuid4 / time.time clocks, and the module logger.debug)
is a recording stub fed the same scripts the C harness feeds gem_call. Compares
the DECISION SEQUENCE (ordered "trace" of {ev,args,ret} seam events), the
canonical return ("ret") and the post-call mutation window ("state").

UNCLAIMED targets driven live: 27 gemini_native_adapter.py top-level defs +
the GeminiNativeClient._auth_headers / _http_error / effective_output_cap
methods. Ported siblings called by a target (_text_of, _inline_data_part,
_has_function_response, _provider_call_id, _long_decode_cap) stay REAL via AST
extraction, matching the C port's internal composition. Claimed siblings are
SEAMED.

Live defs are AST-extracted from the quarry; the needed module constants are
seeded (_EXPRESS_KEY_ON_STUDIO_GUIDANCE / _STUDIO_KEY_ON_EXPRESS_GUIDANCE /
_INTERRUPTED_RESPONSE_PLACEHOLDER / _BLOCK_REASON_GUIDANCE) and the
GeminiAPIError class is exec'd with __future__ annotations. The heavy quarry
import chain (httpx / asyncio / agent.* runtime) is never imported. The exec
namespace is a registered sys.modules module (py3.14 annotation resolution
needs a real module alias). Fail-closed: 0 cases => exit 1; a MISSING seam
script raises RuntimeError("no script for <name>"); any harness/oracle
exception per case rides as {"__raise__": cls, "msg": str(e)} (BaseException
catch, so SystemExit/KeyboardInterrupt/GeneratorExit pass through as specs,
not exits). Summary: 'ORACLE gemini_transport_flow: N cases, M mismatches'.

Representation contract (both sides, see src/agent/transports/gemini_transport_flow.c):
- scripts: MISSING key -> RuntimeError("no script for <name>") (fail-closed); null ->
  None; {"$seq":[...]} per-call queue (exhaustion raises RuntimeError("script exhausted:
  <name>")); {"__raise__":cls,"msg":m} raises (trace ret is the WHOLE marker); any
  other value reused (a bare ARRAY is a LIST RESULT, not a queue). None-valued seam arg
  keys dropped on both sides.
- uuid4 results are 32-char hex strings materialized into a .hex attribute double;
  time_time results are epoch numbers truncated via int(); gemini_http_error results
  are EXCEPTION DESCRIPTORS {"message": m, ...} materialized to real GeminiAPIError
  instances (the port projects them to {"__raise__":"GeminiAPIError","msg":m} as the
  RETURNED value).
- tuple -> array; None -> null; integral floats collapse to ints; the _SSE_DONE
  object() sentinel projects to the string marker "__SSE_DONE__"; SimpleNamespace
  results project to dicts; uncaught exception -> {"__raise__": cls, "msg": str(e)}
  plus code/status_code/retry_after/details for the real GeminiAPIError raise.
- except Exception sites (_response_text, _inline_data_part) swallow the matching
  family; the BaseException trio passes through. Scripted raises materialize to REAL
  builtin exception instances (str(e) == the scripted msg).
- _response_text response doubles: {"text": s} -> SimpleNamespace; a dict with a str
  "__raise__" -> a raising-text property double; any other value passes raw (the
  AttributeError is swallowed by except Exception).
"""
import __future__
import ast
import base64
import builtins
import copy
import json
import math
import os
import re
import sys
import types
from pathlib import Path

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
# Scrubbed-env defense: derive home from __file__ (repo is parent.parent, home one up).
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "gemini_native_adapter.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "gemini_native_adapter.py")
PORT_SRC = str(_REPO / "src" / "agent" / "transports" / "gemini_transport_flow.c")

NAME = "gemini_transport_flow"

PORTED = [
    "_api_client", "_response_text", "wrong_gemini_surface_guidance", "_text_of",
    "_inline_data_part", "_multimodal_part", "_tool_call_id", "_looks_like_json_schema",
    "_has_function_response", "_merge_alternating", "_function_declaration",
    "_long_decode_cap", "_translate_response_format", "_provider_call_id",
    "_new_call_id", "_dump_call_args", "_usage_from_metadata", "_envelope",
    "_tool_call_ns", "_part_text", "_text_part_signature", "_part_function_call",
    "raise_on_prompt_block", "_parse_sse_line", "_tool_call_slot", "_error_info",
    "_error_object",
]
METHODS = ["_auth_headers", "_http_error", "effective_output_cap"]
CONSTANTS = [
    "_EXPRESS_KEY_ON_STUDIO_GUIDANCE", "_STUDIO_KEY_ON_EXPRESS_GUIDANCE",
    "_INTERRUPTED_RESPONSE_PLACEHOLDER", "_BLOCK_REASON_GUIDANCE",
]
WANTED = set(PORTED)
FUNCS = set(PORTED) | {"GeminiNativeClient." + m for m in METHODS}

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src, flags=0):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=flags, dont_inherit=True)


DEF_CODE = {}
METHOD_CODE = {}
CONST_CODE = {}
ERR_CLASS_CODE = None
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, SRC)
    elif isinstance(_node, ast.ClassDef) and _node.name == "GeminiNativeClient":
        for _b in _node.body:
            if isinstance(_b, ast.FunctionDef) and _b.name in METHODS:
                _m = copy.copy(_b)
                _m.decorator_list = []   # strip @staticmethod
                METHOD_CODE[_b.name] = _compile_node(_m, SRC)
    elif isinstance(_node, ast.ClassDef) and _node.name == "GeminiAPIError":
        ERR_CLASS_CODE = _compile_node(_node, SRC, flags=_FLAGS)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(tg, ast.Name) and tg.id in CONSTANTS:
            CONST_CODE[tg.id] = _compile_node(_node, SRC)
_missing = ((WANTED - set(DEF_CODE)) | (set(METHODS) - set(METHOD_CODE))
            | (set(CONSTANTS) - set(CONST_CODE))
            | ({"GeminiAPIError"} if ERR_CLASS_CODE is None else set()))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

# ── canonical projection ────────────────────────────────────────────────────


def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if v is _SSE_DONE:
        return "__SSE_DONE__"
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items()}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, float) and math.isfinite(v) and v.is_integer():
        return int(v)
    return v


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


# ── scripted seams (fail-closed) ────────────────────────────────────────────
_SCRIPTS = None


class Scripts:
    def __init__(self, io, trace):
        self.io = io or {}
        self.trace = trace
        self.cursors = {}

    def next(self, name):
        # Fail-closed: a MISSING script raises; a scripted null is a valid None.
        if name not in self.io:
            return {"__raise__": "RuntimeError", "msg": "no script for " + name}
        script = self.io[name]
        if isinstance(script, dict) and "$seq" in script:
            seq = script["$seq"]
            i = self.cursors.get(name, 0)
            if i >= len(seq):
                return {"__raise__": "RuntimeError", "msg": "script exhausted: " + name}
            self.cursors[name] = i + 1
            return seq[i]
        return script


SEAM_ARGS = {
    "hermes_user_agent": lambda a, k: {"product": a[0]},
    "is_vertex_express_base_url": lambda a, k: {"base_url": a[0]},
    "prepare_gemini_tool_parameters": lambda a, k: {"parameters": a[0]},
    "sanitize_gemini_tool_parameters": lambda a, k: {"parameters": a[0]},
    "uuid4": lambda a, k: {},
    "time_time": lambda a, k: {},
    "logger_debug": lambda a, k: {"msg": a[0], "arg": a[1]},
    "gemini_http_error": lambda a, k: {"response": a[0], "body_text": k.get("body_text"),
                                       "api_key": k["api_key"], "base_url": k["base_url"]},
    "_effective_gemini_max_output_tokens": lambda a, k: {"max_tokens": a[0],
                                                         "thinking_config": a[1]},
}


def _build_http_error(r):
    if isinstance(r, dict) and "message" in r:
        return _BASE_NS["GeminiAPIError"](
            r["message"], code=r.get("code", "gemini_api_error"),
            status_code=r.get("status_code"), retry_after=r.get("retry_after"),
            details=r.get("details"))
    return r


# Results consumed via attribute reads / exception identity need doubles.
BUILD = {
    "uuid4": lambda r: types.SimpleNamespace(hex=r) if isinstance(r, str) else r,
    "gemini_http_error": _build_http_error,
}


def record(name, args):
    scripts = _SCRIPTS
    args = {k: norm(v) for k, v in args.items() if v is not None}
    script = scripts.next(name)
    if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
        marker = dict(script)
        scripts.trace.append({"ev": name, "args": args, "ret": marker})
        raise make_exc(marker)
    result = BUILD[name](script) if name in BUILD else script
    scripts.trace.append({"ev": name, "args": args, "ret": norm(script)})
    return result


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **kw):
        return record(self.name, SEAM_ARGS[self.name](a, kw))


# ── base namespace (defs exec'd once; SeamFn recorders read per-case _SCRIPTS) ──
_SSE_DONE = object()   # the live module's terminal-frame sentinel
_BASE_NS = {
    "__name__": "gem_live_gemini_transport",
    "__builtins__": builtins,
    "json": json, "base64": base64,
    "SimpleNamespace": types.SimpleNamespace,   # _envelope's cls= default evaluates at def time
    "Any": object,
    "uuid": types.SimpleNamespace(uuid4=SeamFn("uuid4")),
    "time": types.SimpleNamespace(time=SeamFn("time_time")),
    "logger": types.SimpleNamespace(debug=SeamFn("logger_debug")),
    "_SSE_DONE": _SSE_DONE,
}
_MOD = types.ModuleType("gem_live_gemini_transport")
_MOD.__dict__.update(_BASE_NS)
sys.modules["gem_live_gemini_transport"] = _MOD
_BASE_NS = _MOD.__dict__
for _nm, _code in CONST_CODE.items():
    exec(_code, _BASE_NS)
exec(ERR_CLASS_CODE, _BASE_NS)
for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)
for _nm, _code in METHOD_CODE.items():
    exec(_code, _BASE_NS)

# Module-top imported collaborators + claimed siblings -> ns-global recorders.
_COLLABS = [
    "hermes_user_agent", "is_vertex_express_base_url",
    "prepare_gemini_tool_parameters", "sanitize_gemini_tool_parameters",
    "gemini_http_error", "_effective_gemini_max_output_tokens",
]
for _nm in _COLLABS:
    _BASE_NS[_nm] = SeamFn(_nm)


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    trace = []
    _SCRIPTS = Scripts(inp.get("io") or {}, trace)
    return _BASE_NS, trace, _SCRIPTS


# ── input doubles ───────────────────────────────────────────────────────────


class _RaisingText:
    """response double whose .text attribute access raises the scripted exc."""

    def __init__(self, exc):
        self._exc = exc

    @property
    def text(self):
        raise self._exc


def mk_response(r):
    if isinstance(r, dict) and isinstance(r.get("__raise__"), str):
        return _RaisingText(make_exc(r))
    if isinstance(r, dict):
        return types.SimpleNamespace(**r)
    return r


def mk_self(d):
    return types.SimpleNamespace(**(d or {}))


# ── live driver ─────────────────────────────────────────────────────────────


def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    if func == "_api_client":
        return ns[func]()
    if func == "_response_text":
        return ns[func](mk_response(inp.get("response")))
    if func == "wrong_gemini_surface_guidance":
        return ns[func](inp.get("base_url"), inp.get("api_key"), inp.get("err_status"))
    if func == "_text_of":
        return ns[func](inp.get("item"))
    if func == "_inline_data_part":
        return ns[func](inp.get("item"))
    if func == "_multimodal_part":
        return ns[func](inp.get("item"))
    if func == "_tool_call_id":
        return ns[func](inp.get("tool_call"))
    if func == "_looks_like_json_schema":
        return ns[func](inp.get("node"))
    if func == "_has_function_response":
        return ns[func](inp.get("content"))
    if func == "_merge_alternating":
        contents = inp.get("contents")
        holder["win"] = ("contents", contents)
        return ns[func](contents)
    if func == "_function_declaration":
        return ns[func](inp.get("tool"), json_schema=inp.get("json_schema", False))
    if func == "_long_decode_cap":
        return ns[func](inp.get("max_tokens"), inp.get("extra_body"))
    if func == "_translate_response_format":
        return ns[func](inp.get("response_format"), json_schema=inp.get("json_schema", False))
    if func == "_provider_call_id":
        return ns[func](inp.get("fc"))
    if func == "_new_call_id":
        return ns[func](inp.get("fc"))
    if func == "_dump_call_args":
        return ns[func](inp.get("fc"), **(inp.get("kwargs") or {}))
    if func == "_usage_from_metadata":
        return ns[func](inp.get("usage_meta"))
    if func == "_envelope":
        return ns[func](inp.get("model", ""), inp.get("object_", ""),
                        inp.get("choice"), inp.get("usage"))
    if func == "_tool_call_ns":
        return ns[func](inp.get("name", ""), inp.get("arguments", ""),
                        inp.get("index", 0), inp.get("call_id", ""),
                        inp.get("extra_content"))
    if func == "_part_text":
        return ns[func](inp.get("part"))
    if func == "_text_part_signature":
        return ns[func](inp.get("part"))
    if func == "_part_function_call":
        return ns[func](inp.get("part"))
    if func == "raise_on_prompt_block":
        return ns[func](inp.get("payload"))
    if func == "_parse_sse_line":
        return ns[func](inp.get("line"))
    if func == "_tool_call_slot":
        return ns[func](inp.get("fc"), inp.get("part"), inp.get("part_index", 0),
                        inp.get("args_str", ""), inp.get("tool_call_indices"))
    if func == "_error_info":
        return ns[func](inp.get("err_obj"))
    if func == "_error_object":
        return ns[func](inp.get("body_text"))
    if func == "GeminiNativeClient._auth_headers":
        return ns["_auth_headers"](mk_self(inp.get("self")))
    if func == "GeminiNativeClient._http_error":
        return ns["_http_error"](mk_self(inp.get("self")), inp.get("response"),
                                 body_text=inp.get("body_text"))
    if func == "GeminiNativeClient.effective_output_cap":
        return ns["effective_output_cap"](max_tokens=inp.get("max_tokens"),
                                          extra_body=inp.get("extra_body"))
    raise KeyError(f"unknown func {func}")


def echo_state(holder):
    w = holder.get("win")
    if not w:
        return {}
    kind, obj = w
    return {kind: norm(obj)}


def raise_spec(e):
    spec = {"__raise__": type(e).__name__, "msg": str(e)}
    if isinstance(e, _BASE_NS["GeminiAPIError"]):
        spec["code"] = e.code
        spec["status_code"] = norm(e.status_code)
        spec["retry_after"] = norm(e.retry_after)
        spec["details"] = norm(e.details)
    return spec


def main():
    n = mism = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                out = {"ret": norm(ret), "trace": trace, "state": echo_state(holder)}
            except BaseException as e:  # noqa: BLE001 - fail-closed per case
                out = {"ret": raise_spec(e), "trace": trace,
                       "state": echo_state(holder)}
        except BaseException as e:  # noqa: BLE001 - seeding failure is a mismatch
            out = {"ret": raise_spec(e), "trace": [], "state": {}}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:800]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:800]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


# ── self-test: drive every fixture, flag drive failures / suspicious raises ──


def cross_check(fixtures):
    """Three-way seam-name cross-check + live-constant assertions."""
    bad = 0
    ctext = Path(PORT_SRC).read_text(encoding="utf-8")
    c_names = set()
    for m in re.finditer(r"gem_call\(ctx,(.*?)\)\s*;", ctext, re.S):
        c_names |= set(re.findall(r'"([A-Za-z_][A-Za-z_0-9]*)"', m.group(1)))
    if c_names != set(SEAM_ARGS):
        bad += 1
        print(f"CROSSCHECK seam-name drift: C-only={sorted(c_names - set(SEAM_ARGS))} "
              f"oracle-only={sorted(set(SEAM_ARGS) - c_names)}")
    fx_io = set()
    for rec in fixtures:
        fx_io |= set((rec["in"].get("io") or {}).keys())
    if fx_io - c_names:
        bad += 1
        print(f"CROSSCHECK fixture io keys with no C seam: {sorted(fx_io - c_names)}")
    fx_funcs = {rec["func"] for rec in fixtures}
    if fx_funcs - FUNCS:
        bad += 1
        print(f"CROSSCHECK fixture funcs not extracted: {sorted(fx_funcs - FUNCS)}")
    if FUNCS - fx_funcs:
        bad += 1
        print(f"CROSSCHECK extracted funcs with no fixture: {sorted(FUNCS - fx_funcs)}")
    checks = [
        ("_INTERRUPTED_RESPONSE_PLACEHOLDER", _BASE_NS["_INTERRUPTED_RESPONSE_PLACEHOLDER"],
         "[The previous response was interrupted before it completed.]"),
        ("_BLOCK_REASON_GUIDANCE", _BASE_NS["_BLOCK_REASON_GUIDANCE"],
         {"BLOCKLIST": "The prompt includes blocked terms. Rephrase and try again.",
          "PROHIBITED_CONTENT": "The prompt may contain prohibited content. Adjust it and try again.",
          "SAFETY": "The prompt was blocked by Gemini's safety filters. Adjust it and try again.",
          "IMAGE_SAFETY": "The image was blocked for safety reasons."}),
    ]
    for nm, got, want in checks:
        if got != want:
            bad += 1
            print(f"CROSSCHECK constant {nm} drifted: {got!r} != {want!r}")
    for nm in ("_EXPRESS_KEY_ON_STUDIO_GUIDANCE", "_STUDIO_KEY_ON_EXPRESS_GUIDANCE"):
        v = _BASE_NS[nm]
        if not (isinstance(v, str) and v.startswith("\n\n") and v.endswith(".")):
            bad += 1
            print(f"CROSSCHECK constant {nm} shape drifted: {v!r}")
    return bad


def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    bad = cross_check(fixtures)
    names = FUNCS | set(SEAM_ARGS) | set(PORTED) | set(METHODS)
    for i, rec in enumerate(fixtures):
        holder = {}
        try:
            ns, trace, scripts = build_ns(rec)
            try:
                ret = drive(rec, ns, scripts, holder)
                spec = norm(ret)
            except BaseException as e:  # noqa: BLE001
                spec = {"__raise__": type(e).__name__, "msg": str(e)}
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                msg = spec.get("msg", "")
                if any(f"'{nm}'" in msg for nm in names):
                    bad += 1
                    print(f"SELFTEST case {i} {rec['func']}: AttributeError naming ported/seam: {msg}")
            if isinstance(spec, dict) and spec.get("__raise__") == "NameError":
                bad += 1
                print(f"SELFTEST case {i} {rec['func']}: NameError (seeding gap): {spec.get('msg', '')}")
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {rec['func']}: DRIVE FAILURE {type(e).__name__}: {e}")
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = Path(__file__).resolve().parent / "fixtures" / "gemini_transport_flow.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()