#!/usr/bin/env python3
"""Oracle for the Telegram adapter FLOW part 7 port
(src/gateway/platforms/telegram_adapter_flow7.c).

Drives the LIVE plugins/platforms/telegram/adapter.py error-classification /
exception-graph / sender-attribution decision surface with SCRIPTED seams.
Claimed siblings / unported collaborators are recording stubs fed the SAME
scripts the C harness feeds its universal seam.  Compares the ordered seam
trace + final return + the state echo.

The live defs are AST-extracted from the adapter module body and the
TelegramAdapter class body (decorators excluded by lineno, segments dedented);
every exec namespace is a registered sys.modules module (py3.14 annotation
resolution).  The heavy quarry import chain is never imported and `telegram`
is blocked so every `from telegram.error import ...` takes its ImportError
fallback (matching the C transcription).

Fail-closed: missing defs or 0 cases => exit 2.  Per-case wrapper catches
BaseException (SystemExit included).  Summary:
'ORACLE telegram_adapter_flow7: N cases, M mismatches'.

Representation contract (shared with the C port): see the port header.
Notably: exception args are eids in `in.errors`; seam args that carry a live
exception use canon {"__raise__": cls, "message": str(exc)}; sets sorted;
float integral values render as ints.
"""
import ast
import builtins
import enum
import json
import os
import re
import sys
import textwrap
import types
from pathlib import Path
from types import SimpleNamespace
import __future__ as _fut

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
A_SRC = os.path.join(Q, "plugins", "platforms", "telegram", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

# name -> "instance" (self passed explicitly) or "free" (module/static fn or
# classmethod whose cls is passed explicitly).  Used for extraction + PoP check.
KINDS = {
    "_iter_exception_graph": "free",
    "_exception_graph_matches": "free",
    "_looks_like_auth_error": "free",
    "_looks_like_network_error": "free",
    "_looks_like_connect_timeout": "free",
    "_looks_like_pool_timeout": "free",
    "_is_thread_not_found_error": "free",
    "_is_bad_request_error": "free",
    "_is_reply_anchor_missing_error": "free",
    "_is_timed_out": "free",
    "_is_rich_capability_error": "instance",
    "_is_rich_fallback_error": "instance",
    "_should_retry_without_dm_topic_reply_anchor": "free",
    "_is_own_message": "instance",
    "_sender_is_other_bot": "instance",
    "_redact_telegram_error_text": "free",
}
# EXACTLY the C ta7_call seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    "_exception_graph_matches", "_dm_topic_fallback", "_is_bad_request_error",
    "_is_rich_capability_error", "_is_reply_anchor_missing_error",
    "describe_proxy_error", "redact_sensitive_text",
]


def seg(src, node):
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def extract():
    mod = new_module("ta7_oracle_adapter")
    ns = mod.__dict__
    ns.update({"os": None, "json": json, "re": re})
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "TelegramAdapter")
    found = set()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in KINDS:
            exec_node(src, node, ns, A_SRC)
            found.add(node.name)
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in KINDS:
            exec_node(src, node, ns, A_SRC)
            found.add(node.name)
    missing = [k for k in KINDS if k not in ns]
    if missing:
        print(f"FATAL oracle_telegram_adapter_flow7: missing: {missing}")
        sys.exit(2)
    # Block telegram imports so every `from telegram.error import ...` takes
    # its ImportError fallback (deterministic, mirrors the C transcription).
    sys.modules["telegram"] = None
    return ns, found


NS, _FOUND = extract()
REAL_ITER = NS["_iter_exception_graph"]


# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, enum.Enum):
            return x.value
        if isinstance(x, BaseException):
            return {"__raise__": type(x).__name__, "message": str(x)}
        if isinstance(x, types.SimpleNamespace):
            return {k: conv(val) for k, val in vars(x).items()}
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C ta7_call exactly) ──
_MISSING = object()
_CUR = None


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return ret


def rec(name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = _CUR.call(name, args)
        return build(raw) if build else raw
    return f


# ── exception registry (live exception objects with attributes + graph) ──
def build_errors(spec):
    spec = spec or {}
    classes, objs = {}, {}
    for eid, e in spec.items():
        name = e.get("class", "Exception")
        cls = getattr(builtins, name, None)
        if not (isinstance(cls, type) and issubclass(cls, BaseException)):
            base = getattr(builtins, e.get("base", "Exception"), Exception)
            if not (isinstance(base, type) and issubclass(base, BaseException)):
                base = Exception
            cls = type(name, (base,), {})
        classes[eid] = cls
    for eid, e in spec.items():
        obj = classes[eid](e.get("message", ""))
        for k, v in e.items():
            if k not in ("class", "message", "base", "cause", "context"):
                setattr(obj, k, v)
        objs[eid] = obj
    for eid, e in spec.items():
        for key in ("cause", "context"):
            ref = e.get(key)
            if isinstance(ref, str) and ref in objs:
                setattr(objs[eid], "__" + key + "__", objs[ref])
    return objs


def ns_double(v):
    if isinstance(v, dict):
        return SimpleNamespace(**{k: ns_double(x) for k, x in v.items()})
    if isinstance(v, list):
        return [ns_double(x) for x in v]
    return v


def mat_state(k, v):
    if k == "_bot" and isinstance(v, dict):
        return SimpleNamespace(**v)
    return v


# ── seam stubs (explicit literal names; cross_check scans these) ──
def build_seams():
    st = {}
    st["_exception_graph_matches"] = rec("_exception_graph_matches",
        lambda error, name_marker, *text_markers: {
            "error": error, "name_marker": name_marker, "text_markers": list(text_markers)})
    st["_dm_topic_fallback"] = rec("_dm_topic_fallback", lambda metadata: {"metadata": metadata})
    st["_is_bad_request_error"] = rec("_is_bad_request_error", lambda error: {"error": error})
    st["_is_rich_capability_error"] = rec("_is_rich_capability_error", lambda exc: {"exc": exc})
    st["_is_reply_anchor_missing_error"] = rec("_is_reply_anchor_missing_error", lambda error: {"error": error})
    st["describe_proxy_error"] = rec("describe_proxy_error",
        lambda error, proxy_url: {"error": error, "proxy_url": proxy_url})
    st["redact_sensitive_text"] = rec("redact_sensitive_text",
        lambda text, force: {"text": text, "force": force})
    return st


def make_doubles(state, stubs):
    """Return (self_double, cls_double).  Seam recorders live on the doubles
    as staticmethods (so no implicit self/cls is bound into their *args); the
    ported functions themselves are always called from NS directly."""
    inst_attrs = {}
    for name in SEAM_NAMES:
        inst_attrs[name] = staticmethod(stubs[name])
    D = type("TelegramAdapter", (), inst_attrs)
    inst = D()
    for k, v in (state or {}).items():
        setattr(inst, k, mat_state(k, v))
    cls_attrs = {}
    for name in SEAM_NAMES:
        cls_attrs[name] = staticmethod(stubs[name])
    cls = type("TelegramAdapterCls", (), cls_attrs)
    return inst, cls


def drive(func, inp):
    global _CUR
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    _CUR = ctx
    objs = build_errors(inp.get("errors"))
    stubs = build_seams()

    # in-function imports resolve through sys.modules.
    for n in ("agent",):
        m = types.ModuleType(n)
        m.__path__ = []
        sys.modules[n] = m
    am = types.ModuleType("agent.redact")
    am.redact_sensitive_text = stubs["redact_sensitive_text"]
    sys.modules["agent.redact"] = am
    NS["describe_proxy_error"] = stubs["describe_proxy_error"]
    NS["redact_sensitive_text"] = stubs["redact_sensitive_text"]

    inst, cls = make_doubles(state, stubs)

    def E(k):
        v = args.get(k)
        return objs.get(v) if isinstance(v, str) else None

    def A(k):
        return ns_double(args.get(k))

    try:
        if func == "_iter_exception_graph":
            ret = list(REAL_ITER(E("error")))
        elif func == "_exception_graph_matches":
            ret = NS[func](E("error"), args.get("name_marker", ""), *args.get("text_markers", []))
        elif func in ("_looks_like_auth_error", "_looks_like_network_error",
                      "_looks_like_pool_timeout", "_is_thread_not_found_error",
                      "_is_bad_request_error", "_is_reply_anchor_missing_error",
                      "_is_timed_out"):
            ret = NS[func](E("error") if "error" in args else E("exc"))
        elif func == "_looks_like_connect_timeout":
            ret = NS[func](cls, E("error"))
        elif func == "_is_rich_capability_error":
            ret = NS[func](inst, E("exc"))
        elif func == "_is_rich_fallback_error":
            ret = NS[func](inst, E("exc"))
        elif func == "_should_retry_without_dm_topic_reply_anchor":
            ret = NS[func](cls, E("error"), args.get("metadata"), args.get("reply_to_message_id"))
        elif func == "_is_own_message":
            ret = NS[func](inst, A("message"))
        elif func == "_sender_is_other_bot":
            ret = NS[func](inst, A("message"))
        elif func == "_redact_telegram_error_text":
            ret = NS[func](E("error"), args.get("proxy_url"))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(vars(inst))}


# ── cross-checks (C seams / fixtures / C funcs / PoP) ──
def cross_check():
    csrc = (_REPO / "src" / "gateway" / "platforms" /
            "telegram_adapter_flow7.c").read_text(encoding="utf-8")
    c_seams = set(re.findall(r'ta7_call\(\s*ctx,\s*"([^"]+)"', csrc))
    if c_seams != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow7: C seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - c_seams)} extra={sorted(c_seams - set(SEAM_NAMES))}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'_CUR\.call\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'rec\(\s*"([^"]+)"', osrc))
    if used != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow7: oracle seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - used)} extra={sorted(used - set(SEAM_NAMES))}")
        sys.exit(2)
    pops = set(re.findall(r'PoP:\s*ta7_\w+\s*@\s*plugins/platforms/telegram/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_telegram_adapter_flow7: PoP/KINDS drift: c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow7.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in fix.open(encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func", ""))
        if fix_keys != set(SEAM_NAMES):
            print(f"FATAL oracle_telegram_adapter_flow7: fixture io key set != SEAM_NAMES: "
                  f"missing={sorted(set(SEAM_NAMES) - fix_keys)} extra={sorted(fix_keys - set(SEAM_NAMES))}")
            sys.exit(2)
        if fix_funcs != set(KINDS):
            print(f"FATAL oracle_telegram_adapter_flow7: fixture func set != KINDS: "
                  f"missing={sorted(set(KINDS) - fix_funcs)} extra={sorted(fix_funcs - set(KINDS))}")
            sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 12:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec_in = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_in.get("func", "")
        inp = rec_in.get("in") or {}
        got = rec_in.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}")
            continue
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown)
            shown += 1
    print(f"ORACLE telegram_adapter_flow7: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow7.jsonl"
    n = bad = 0
    counts = {}
    seam_attrs = set(SEAM_NAMES) | set(KINDS)
    for line in fix.open(encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            out = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = out["ret"]
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError" and \
                any(f"'{nm}'" in str(r.get("message", "")) for nm in seam_attrs):
            bad += 1
            print(f"SELFTEST BINDING GAP {func} case {n}: {r['message']}")
            continue
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            if not (case.get("in") or {}).get("expect_fail_closed"):
                bad += 1
                print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
    if set(counts) != set(KINDS):
        print(f"SELFTEST KINDS DRIFT: fixture funcs={sorted(counts)} kinds={sorted(KINDS)}")
        bad += 1
    print(f"SELFTEST telegram_adapter_flow7: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
