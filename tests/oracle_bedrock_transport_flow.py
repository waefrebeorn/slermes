#!/usr/bin/env python3
"""Oracle for the Bedrock Converse transport UNCLAIMED-remainder FLOW port
(src/agent/transports/bedrock_transport_flow.c).

The task names agent/transports/bedrock.py; that module EXISTS in this quarry
snapshot but is a thin delegation shell whose every method is already CLAIMED
by transports_bedrock_ports.c (brt_*). The UNCLAIMED functions of the
transport's data path live in the module it delegates to,
agent/bedrock_adapter.py ("AWS Bedrock Converse API adapter"), so live defs are
AST-extracted from there and the PoP lines credit that REAL defining module.

Drives the LIVE defs with SCRIPTED seams: every collaborator (claimed siblings
parse_bedrock_openai_model_id / _decode_redacted / _parse_tool_args /
_converse_replay_policy / strip_redacted_reasoning / is_anthropic_bedrock_model /
_get_bedrock_control_client / resolve_bedrock_region / _extract_provider_from_arn,
the foreign agent.anthropic_adapter._thinking_kwargs import, the boto3 client
methods get_inference_profile / list_foundation_models / list_inference_profiles,
and the module logger.warning / logger.debug) is a recording stub fed the same
scripts the C harness feeds bt2_call. Compares the DECISION SEQUENCE (ordered
"trace" of {ev,args,ret} seam events), the canonical return ("ret") and the
post-call mutation window ("state").

UNCLAIMED targets driven live: 27 bedrock_adapter.py top-level defs. Ported
siblings called by a target (_reasoning_text_block, _redacted_reasoning_block,
_tool_use_block, _issuer, _minted_outside_claude,
_resolve_inference_profile_model_id, _claude_converse_reasoning,
_nova_converse_reasoning, _model_entry) stay REAL via AST extraction, matching
the C port's internal composition. Claimed siblings are SEAMED.

Live defs are AST-extracted from the quarry; the needed module constants are
seeded (CONVERSE_ISSUER_TYPE / _NOVA_EFFORT / _ANTHROPIC_TO_CONVERSE /
_CONVERSE_REASONING_BUILDERS / CACHE_POINT_PLACEMENTS /
_APPLICATION_PROFILE_ARN_RE / _ARN_REGION_RE /
_REDACTED_REASONING_REJECTION_PATTERN). The heavy quarry import chain (boto3 /
httpx / pm / agent.* runtime) is never imported; agent.anthropic_adapter rides
a sys.modules stub. The exec namespace is a registered sys.modules module
(py3.14 annotation resolution needs a real module alias). Fail-closed: 0 cases
=> exit 1; a MISSING seam script raises RuntimeError("no script for <name>");
any harness/oracle exception per case rides as {"__raise__": cls, "msg": str(e)}
(BaseException catch, so SystemExit/KeyboardInterrupt/GeneratorExit pass
through as specs, not exits). Summary:
'ORACLE bedrock_transport_flow: N cases, M mismatches'.

Representation contract (both sides, see
src/agent/transports/bedrock_transport_flow.c):
- scripts: MISSING key -> RuntimeError("no script for <name>") (fail-closed);
  null -> None; {"$seq":[...]} per-call queue (exhaustion raises
  RuntimeError("script exhausted: <name>")); {"__raise__":cls,"msg":m} raises
  (trace ret is the WHOLE marker); any other value reused (a bare ARRAY is a
  LIST RESULT, not a queue). None-valued seam arg keys dropped on both sides.
- parse_bedrock_openai_model_id results: arrays materialize to tuples;
  _decode_redacted results: {"__bytes__":[...]} markers materialize to real
  bytes; _get_bedrock_control_client results: null -> None, anything else ->
  a recording client double; strip_redacted_reasoning results:
  {"__same__":true} -> the SAME kwargs object (identity), else a new value.
- tuple -> array; None -> null; integral floats collapse to ints;
  bytes/bytearray -> {"__bytes__":[int,...]}; SimpleNamespace results project
  to dicts; uncaught exception -> {"__raise__": cls, "msg": str(e)}.
- except Exception sites (_resolve_inference_profile_model_id) swallow the
  matching family; the BaseException trio passes through. Scripted raises
  materialize to REAL builtin exception instances (str(e) == the scripted msg).
- module state (_CACHE_POINT_REJECTIONS, _inference_profile_model_cache) is
  seeded per case from in.state and echoed for the keys PRESENT in the fixture
  state only; rejection placement sets echo sorted on both sides.
- logger seam args are stringified (live %s formatting); logger_debug's arg2
  is str(exc) of the caught exception.
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
if not (QUARRY / "agent" / "bedrock_adapter.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "bedrock_adapter.py")
PORT_SRC = str(_REPO / "src" / "agent" / "transports" / "bedrock_transport_flow.c")

NAME = "bedrock_transport_flow"

PORTED = [
    "bedrock_openai_uses_mantle", "_tool_use_block", "_reasoning_text_block",
    "_redacted_reasoning_block", "_anthropic_ordered_blocks", "_issuer",
    "_minted_outside_claude", "reasoning_minted_outside_claude",
    "_unsigned_reasoning", "_verbatim", "_without_reasoning", "_filter_reasoning",
    "_without_cache_points", "_without_redacted_reasoning",
    "note_cache_point_rejection", "reset_cache_point_rejections",
    "recover_from_redacted_reasoning_rejection", "_encode_redacted",
    "_tool_call_ns", "_persisted_reasoning_details", "_model_entry",
    "_nova_converse_reasoning", "_claude_converse_reasoning",
    "_apply_converse_reasoning", "_resolve_inference_profile_model_id",
    "_list_foundation_models", "_list_inference_profiles",
]
CONSTANTS = [
    "CONVERSE_ISSUER_TYPE", "_NOVA_EFFORT", "_ANTHROPIC_TO_CONVERSE",
    "_CONVERSE_REASONING_BUILDERS", "CACHE_POINT_PLACEMENTS",
    "_APPLICATION_PROFILE_ARN_RE", "_ARN_REGION_RE",
    "_REDACTED_REASONING_REJECTION_PATTERN",
]
WANTED = set(PORTED)
FUNCS = set(PORTED)

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src, flags=0):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=flags, dont_inherit=True)


DEF_CODE = {}
CONST_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, SRC, flags=_FLAGS)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(tg, ast.Name) and tg.id in CONSTANTS:
            CONST_CODE[tg.id] = _compile_node(_node, SRC)
_missing = (WANTED - set(DEF_CODE)) | (set(CONSTANTS) - set(CONST_CODE))
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

# ── canonical projection ────────────────────────────────────────────────────


def norm(v):
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, (bytes, bytearray)):
        return {"__bytes__": list(v)}
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
    "parse_bedrock_openai_model_id": lambda a, k: {"model_id": a[0]},
    "_decode_redacted": lambda a, k: {"encoded": a[0]},
    "_parse_tool_args": lambda a, k: {"args": a[0]},
    "_converse_replay_policy": lambda a, k: {"model": a[0]},
    "strip_redacted_reasoning": lambda a, k: {"kwargs": a[0]},
    "logger_warning": lambda a, k: {"msg": a[0], "arg": str(a[1])},
    "logger_debug": lambda a, k: {"msg": a[0], "arg1": a[1], "arg2": str(a[2])},
    "_thinking_kwargs": lambda a, k: {"reasoning_config": a[0], "model": a[1], "max_tokens": a[2]},
    "is_anthropic_bedrock_model": lambda a, k: {"model_id": a[0]},
    "_get_bedrock_control_client": lambda a, k: {"region": a[0]},
    "get_inference_profile": lambda a, k: {"inferenceProfileIdentifier": k["inferenceProfileIdentifier"]},
    "resolve_bedrock_region": lambda a, k: {},
    "list_foundation_models": lambda a, k: {},
    "list_inference_profiles": lambda a, k: {"nextToken": k.get("nextToken")},
    "_extract_provider_from_arn": lambda a, k: {"arn": a[0]},
}


class _ClientDouble:
    """boto3 client double: every method is a recording seam (instance attrs,
    so no self binds)."""

    def __init__(self):
        self.get_inference_profile = SeamFn("get_inference_profile")
        self.list_foundation_models = SeamFn("list_foundation_models")
        self.list_inference_profiles = SeamFn("list_inference_profiles")


def _build_parse(r, args):
    return tuple(r) if isinstance(r, list) else r


def _build_decode(r, args):
    if isinstance(r, dict) and "__bytes__" in r:
        return bytes(r["__bytes__"])
    return r


def _build_control_client(r, args):
    return None if r is None else _ClientDouble()


def _build_strip(r, args):
    if isinstance(r, dict) and r.get("__same__") is True:
        return args.get("kwargs")   # the SAME object (identity contract)
    return r


# Results consumed via attribute reads / exception identity need doubles.
BUILD = {
    "parse_bedrock_openai_model_id": _build_parse,
    "_decode_redacted": _build_decode,
    "_get_bedrock_control_client": _build_control_client,
    "strip_redacted_reasoning": _build_strip,
}


def record(name, args):
    scripts = _SCRIPTS
    args_norm = {k: norm(v) for k, v in args.items() if v is not None}
    script = scripts.next(name)
    if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
        marker = dict(script)
        scripts.trace.append({"ev": name, "args": args_norm, "ret": marker})
        raise make_exc(marker)
    result = BUILD[name](script, args) if name in BUILD else script
    scripts.trace.append({"ev": name, "args": args_norm, "ret": norm(script)})
    return result


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **kw):
        return record(self.name, SEAM_ARGS[self.name](a, kw))


# ── base namespace (defs exec'd once; SeamFn recorders read per-case _SCRIPTS) ──
_BASE_NS = {
    "__name__": "bt2_live_bedrock_transport",
    "__builtins__": builtins,
    "json": json, "base64": base64, "re": re,
    "SimpleNamespace": types.SimpleNamespace,
    "logger": types.SimpleNamespace(warning=SeamFn("logger_warning"),
                                    debug=SeamFn("logger_debug")),
}
_MOD = types.ModuleType("bt2_live_bedrock_transport")
_MOD.__dict__.update(_BASE_NS)
sys.modules["bt2_live_bedrock_transport"] = _MOD
_BASE_NS = _MOD.__dict__

# Claimed siblings + foreign collaborators -> ns-global recorders (installed
# BEFORE the constants: _CONVERSE_REASONING_BUILDERS captures the current
# is_anthropic_bedrock_model binding at exec time).
_COLLABS = [
    "parse_bedrock_openai_model_id", "_decode_redacted", "_parse_tool_args",
    "_converse_replay_policy", "strip_redacted_reasoning",
    "is_anthropic_bedrock_model", "_get_bedrock_control_client",
    "resolve_bedrock_region", "_extract_provider_from_arn",
]
for _nm in _COLLABS:
    _BASE_NS[_nm] = SeamFn(_nm)

# agent.anthropic_adapter._thinking_kwargs rides a sys.modules stub (the
# in-function import must never reach the real quarry package).
_AGENT_MOD = types.ModuleType("agent")
_AGENT_MOD.__path__ = []
_ANTHROPIC_MOD = types.ModuleType("agent.anthropic_adapter")
_ANTHROPIC_MOD._thinking_kwargs = SeamFn("_thinking_kwargs")
_AGENT_MOD.anthropic_adapter = _ANTHROPIC_MOD
sys.modules["agent"] = _AGENT_MOD
sys.modules["agent.anthropic_adapter"] = _ANTHROPIC_MOD

for _nm, _code in DEF_CODE.items():
    exec(_code, _BASE_NS)
for _nm, _code in CONST_CODE.items():
    exec(_code, _BASE_NS)

# Per-case module state (seeded in build_ns, never exec'd as constants).
_BASE_NS["_CACHE_POINT_REJECTIONS"] = {}
_BASE_NS["_inference_profile_model_cache"] = {}


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    trace = []
    _SCRIPTS = Scripts(inp.get("io") or {}, trace)
    st = inp.get("state") or {}
    _BASE_NS["_CACHE_POINT_REJECTIONS"] = {
        k: set(v) for k, v in (st.get("_CACHE_POINT_REJECTIONS") or {}).items()}
    _BASE_NS["_inference_profile_model_cache"] = dict(st.get("_inference_profile_model_cache") or {})
    return _BASE_NS, trace, _SCRIPTS


# ── input doubles ───────────────────────────────────────────────────────────


def mk_client(c):
    return None if c is None else _ClientDouble()


def mk_keep(keep, ns):
    if isinstance(keep, str) and keep in ns and callable(ns[keep]):
        return ns[keep]
    return keep


def mk_exc_arg(exc):
    msg = exc if isinstance(exc, str) else "None"
    return RuntimeError(msg)


# ── live driver ─────────────────────────────────────────────────────────────


def drive(rec, ns, scripts, holder):
    func = rec["func"]
    inp = rec["in"]
    g = inp.get
    if func == "bedrock_openai_uses_mantle":
        return ns[func](g("model_id"))
    if func == "_tool_use_block":
        return ns[func](g("tool_use_id"), g("name"), g("input_dict"))
    if func == "_reasoning_text_block":
        return ns[func](g("text"), g("signature"))
    if func == "_redacted_reasoning_block":
        return ns[func](g("encoded"))
    if func == "_anthropic_ordered_blocks":
        return ns[func](g("msg"))
    if func == "_issuer":
        return ns[func](g("msg"))
    if func == "_minted_outside_claude":
        return ns[func](g("issuer"))
    if func == "reasoning_minted_outside_claude":
        return ns[func](g("msg"))
    if func == "_unsigned_reasoning":
        return ns[func](g("block"))
    if func == "_verbatim":
        return ns[func](g("block"))
    if func == "_without_reasoning":
        return ns[func](g("block"))
    if func == "_filter_reasoning":
        return ns[func](g("blocks"), mk_keep(g("keep"), ns))
    if func == "_without_cache_points":
        return ns[func](g("blocks"))
    if func == "_without_redacted_reasoning":
        return ns[func](g("blocks"))
    if func == "note_cache_point_rejection":
        return ns[func](g("model_id"), g("placement"))
    if func == "reset_cache_point_rejections":
        return ns[func]()
    if func == "recover_from_redacted_reasoning_rejection":
        return ns[func](mk_exc_arg(g("exc")), g("kwargs"))
    if func == "_encode_redacted":
        r = g("redacted")
        if isinstance(r, dict) and "__bytes__" in r:
            r = bytes(r["__bytes__"])
        elif isinstance(r, dict) and "__bytearray__" in r:
            r = bytearray(r["__bytearray__"])
        return ns[func](r)
    if func == "_tool_call_ns":
        return ns[func](g("tool_use_id"), g("name"), g("input_dict"))
    if func == "_persisted_reasoning_details":
        return ns[func](g("ordered_blocks"))
    if func == "_model_entry":
        return ns[func](g("model_id"), g("name"), g("provider"), g("input_mods"), g("output_mods"))
    if func == "_nova_converse_reasoning":
        return ns[func](g("model"), g("reasoning_config"), g("max_tokens"))
    if func == "_claude_converse_reasoning":
        return ns[func](g("model"), g("reasoning_config"), g("max_tokens"))
    if func == "_apply_converse_reasoning":
        kwargs = g("kwargs")
        inference_config = g("inference_config")
        holder["win"] = {"kwargs": kwargs, "inference_config": inference_config}
        return ns[func](kwargs, inference_config, g("model"), g("reasoning_config"), g("max_tokens"))
    if func == "_resolve_inference_profile_model_id":
        region = inp["region"] if "region" in inp else ""
        return ns[func](g("profile_arn"), region)
    if func == "_list_foundation_models":
        models = g("models")
        holder["win"] = {"models": models}
        return ns[func](mk_client(g("client")), set(g("filter_set") or []), models)
    if func == "_list_inference_profiles":
        models = g("models")
        holder["win"] = {"models": models}
        return ns[func](mk_client(g("client")), set(g("filter_set") or []), models)
    raise KeyError(f"unknown func {func}")


def echo_state(holder, inp):
    w = holder.get("win")
    if w is not None:
        return {k: norm(v) for k, v in w.items()}
    st = inp.get("state") or {}
    out = {}
    if "_CACHE_POINT_REJECTIONS" in st:
        out["_CACHE_POINT_REJECTIONS"] = {
            k: sorted(v) for k, v in _BASE_NS["_CACHE_POINT_REJECTIONS"].items()}
    if "_inference_profile_model_cache" in st:
        out["_inference_profile_model_cache"] = dict(_BASE_NS["_inference_profile_model_cache"])
    return out


def raise_spec(e):
    return {"__raise__": type(e).__name__, "msg": str(e)}


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
                out = {"ret": norm(ret), "trace": trace, "state": echo_state(holder, rec["in"])}
            except BaseException as e:  # noqa: BLE001 - fail-closed per case
                out = {"ret": raise_spec(e), "trace": trace,
                       "state": echo_state(holder, rec["in"])}
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
    c_names = set(re.findall(r'bt2_call\(ctx,\s*"([A-Za-z_][A-Za-z_0-9]*)"', ctext))
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
        ("CONVERSE_ISSUER_TYPE", _BASE_NS["CONVERSE_ISSUER_TYPE"], "bedrock.native_assistant"),
        ("_NOVA_EFFORT", _BASE_NS["_NOVA_EFFORT"], {"minimal": "low", "low": "low", "medium": "medium"}),
        ("CACHE_POINT_PLACEMENTS", _BASE_NS["CACHE_POINT_PLACEMENTS"], ("tools", "system", "messages")),
        ("_APPLICATION_PROFILE_ARN_RE", _BASE_NS["_APPLICATION_PROFILE_ARN_RE"].pattern,
         ":application-inference-profile/"),
        ("_ARN_REGION_RE", _BASE_NS["_ARN_REGION_RE"].pattern, r"^arn:[^:]+:bedrock:([a-z0-9-]+):"),
        ("_REDACTED_REASONING_REJECTION_PATTERN",
         _BASE_NS["_REDACTED_REASONING_REJECTION_PATTERN"].pattern,
         r"ValidationException.*(?:redacted|encrypt)"),
    ]
    for nm, got, want in checks:
        if got != want:
            bad += 1
            print(f"CROSSCHECK constant {nm} drifted: {got!r} != {want!r}")
    if _BASE_NS["_ARN_REGION_RE"].flags & re.IGNORECASE == 0:
        bad += 1
        print("CROSSCHECK _ARN_REGION_RE lost IGNORECASE")
    rf = _BASE_NS["_REDACTED_REASONING_REJECTION_PATTERN"].flags
    if not (rf & re.IGNORECASE and rf & re.DOTALL):
        bad += 1
        print("CROSSCHECK _REDACTED_REASONING_REJECTION_PATTERN lost IGNORECASE|DOTALL")
    return bad


def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    bad = cross_check(fixtures)
    names = FUNCS | set(SEAM_ARGS) | set(PORTED)
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
                # null-client method raises are the documented contract (the
                # attribute IS the seam name, raised on None before the seam)
                null_client = msg in (
                    "'NoneType' object has no attribute 'list_foundation_models'",
                    "'NoneType' object has no attribute 'list_inference_profiles'",
                    "'NoneType' object has no attribute 'get_inference_profile'")
                if not null_client and any(f"'{nm}'" in msg for nm in names):
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
        fx = Path(__file__).resolve().parent / "fixtures" / "bedrock_transport_flow.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()