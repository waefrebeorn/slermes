#!/usr/bin/env python3
"""Generate fixtures for tui_methods_session_flow2 AND verify shadow-diff 0.

Two jobs:
  1. build_cases() -> the fixture corpus (6 cases per ported function, 15 funcs).
  2. shadow_diff() -> drive the LIVE AST-extracted quarry functions with
     recording stubs fed the SAME in.io scripts, and diff the resulting
     {ret,trace,state} against the oracle's literal C shadow mirror.  A diff of
     0 proves the mirror (and thus the C port, which the gate diffs against the
     mirror) is faithful to live behaviour on the whole corpus.

Run:  python tests/gen_tui_methods_session_flow2_cases.py            (write fixtures)
      python tests/gen_tui_methods_session_flow2_cases.py --check    (verify only)
"""
import ast
import base64
import copy
import importlib.util
import json
import os
import sys
import textwrap
import types
from pathlib import Path

_REPO = Path(__file__).resolve().parent.parent
Q = Path(os.path.expanduser("~/hermes-agent"))
SRC = Q / "tui_gateway" / "methods_session.py"
FIX = _REPO / "tests" / "fixtures" / "tui_methods_session_flow2.jsonl"
_FLAGS = 0  # future annotations not needed for these signatures

# import the oracle (mirror FN, Ctx, helpers, canon)
_spec = importlib.util.spec_from_file_location(
    "oracle_flow2", _REPO / "tests" / "oracle_tui_methods_session_flow2.py")
oracle = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(oracle)
Ctx = oracle.Ctx
FN = oracle.FN
canon = oracle.canon
is_raise = oracle.is_raise
pystr = oracle.pystr


def S(ret=None, raise_spec=None):
    """Wrap a seam result: __script__ wrapper avoids the bare-array QUEUE trap.
    A dict shaped like R(...) is auto-detected as a raise spec."""
    if raise_spec is not None:
        return {"__script__": True, "raise": raise_spec}
    if isinstance(ret, dict) and set(ret) == {"type", "message"}:
        return {"__script__": True, "raise": ret}
    return {"__script__": True, "ret": ret}


def R(ty, msg):
    return {"type": ty, "message": msg}


# ---------------------------------------------------------------------------
# Fixture corpus.
# ---------------------------------------------------------------------------
def build_cases():
    C = []

    def add(func, args, io=None, state=None):
        rec = {"func": func, "in": {"args": args, "io": io or {}, "state": state or {}}}
        C.append(rec)

    rid = "r1"
    # _b64 (6)
    for s in ["", "a", "ab", "abc", "hello", "Hi!"]:
        add("_b64", {"data": s})

    # terminal.resize (6)
    for cols in [80, 120, "40", 0, True, "0"]:
        add("terminal.resize", {"rid": rid, "params": {"cols": cols}}, state={})

    # delegation.status (6)
    statuses = [
        {"list_active_subagents": [], "is_spawn_paused": False, "_get_max_spawn_depth": 3, "_get_max_concurrent_children": 4},
        {"list_active_subagents": ["a", "b"], "is_spawn_paused": True, "_get_max_spawn_depth": 1, "_get_max_concurrent_children": 2},
        {"list_active_subagents": None, "is_spawn_paused": None, "_get_max_spawn_depth": None, "_get_max_concurrent_children": None},
        {"list_active_subagents": ["z"], "is_spawn_paused": False, "_get_max_spawn_depth": 2, "_get_max_concurrent_children": 2},
        {"list_active_subagents": ["x"], "is_spawn_paused": False, "_get_max_spawn_depth": 0, "_get_max_concurrent_children": 0},
        {"list_active_subagents": ["a"], "is_spawn_paused": True, "_get_max_spawn_depth": 5, "_get_max_concurrent_children": 10},
    ]
    for st in statuses:
        add("delegation.status", {"rid": rid}, io={k: S(v) for k, v in st.items()})

    # delegation.pause (6)
    for case in [
        {}, {"paused": True}, {"paused": False}, {"paused": None},
        {"paused": 0}, {"paused": "yes"},
    ]:
        add("delegation.pause", {"rid": rid, "params": case}, io={"set_spawn_paused": S(True)})

    # session.events.stats (6)
    for val in [{"total": 5}, {}, None, {"seq": 9, "truncated": False}, [], 0]:
        add("session.events.stats", {"rid": rid}, io={"replay_stats": S(val)})

    # session.events.since (6)
    cases_since = [
        ({"session_id": "s1", "last_seen": 0}, {"events_since": [{"n": 1}], "latest_seq": 1, "is_truncated": False, "replay_epoch": 7, "open_requests": []}),
        ({"session_id": "s2", "last_seen": "5"}, {"events_since": [], "latest_seq": 5, "is_truncated": True, "replay_epoch": 7, "open_requests": ["q1"]}),
        ({"last_seen": 0}, {"events_since": [], "latest_seq": 0, "is_truncated": False, "replay_epoch": 1, "open_requests": []}),
        ({"session_id": "s3", "last_seen": "bad"}, {}),
        ({"session_id": "s4", "last_seen": 2}, {"events_since": [{"n": 2}, {"n": 3}], "latest_seq": 0, "is_truncated": False, "replay_epoch": 1, "open_requests": []}),
        ({"session_id": 123, "last_seen": 1}, {"events_since": [{"n": 1}, {"n": 2}], "latest_seq": 2, "is_truncated": False, "replay_epoch": 3, "open_requests": []}),
    ]
    for params, io in cases_since:
        add("session.events.since", {"rid": rid, "params": params}, io={k: S(v) for k, v in io.items()})

    # pet.disable (6)
    for val in [None, {}, False, True, 0, "ok"]:
        add("pet.disable", {"rid": rid}, io={"set_enabled": S(val)})

    # pet.scale (6)
    scales = [
        [0.8, None], [1.0, None], [2, ""], [None, "scale out of range"],
        ["1.5", None], [0, "bad"],
    ]
    for sc in scales:
        add("pet.scale", {"rid": rid, "params": {"scale": 1.5}}, io={"set_pet_scale": S(sc)})

    # pet.cancel (6)
    for params in [
        {}, {"token": ""}, {"token": "abc"}, {"token": "  tok  "},
        {"token": None}, {"token": 123},
    ]:
        add("pet.cancel", {"rid": rid, "params": params}, io={"pet_cancel_request": S(None)})

    # pet.generate.status (6)
    gs = [
        (None, ["a", "b"]),
        (R("GenerationError", "no provider"), []),
        (None, R("RuntimeError", "list failed")),
        ("provider-x", ["p"]),
        (R("GenerationError", "refs required"), R("Exception", "x")),
        (None, []),
    ]
    for rp, lp in gs:
        io = {"resolve_provider": S(rp), "list_sprite_providers": S(lp)}
        add("pet.generate.status", {"rid": rid}, io=io)

    # session.close (6)
    closes = [
        ({"session_id": "s1"}, {"_end_msgs": ["bye"]}, True),
        ({"session_id": "s1"}, {"_end_msgs": []}, False),
        ({"session_id": "s2"}, None, True),
        ({}, {"_end_msgs": ["a", "b"]}, None),
        ({"session_id": "s3"}, {"_end_msgs": "not-a-list"}, False),
        ({"session_id": "s4"}, {"other": 1}, True),
    ]
    for params, session, closed in closes:
        io = {"pop_session_by_id": S(session), "teardown_popped_session": S(closed)}
        add("session.close", {"rid": rid, "params": params}, io=io)

    # session.branch (6)
    for ret in [
        {"jsonrpc": "2.0", "id": rid, "result": {"ok": True}},
        {"jsonrpc": "2.0", "id": rid, "error": {"code": 4001, "message": "no"}},
        R("RuntimeError", "branch boom"),
        None, {}, "str-result",
    ]:
        add("session.branch", {"rid": rid, "params": {"session_id": "s1"}, "session": {"cols": 80}},
            io={"branch_live": S(ret)})

    # session.branch_whole (6)
    for ret in [
        {"jsonrpc": "2.0", "id": rid, "result": {"ok": True}},
        {"jsonrpc": "2.0", "id": rid, "error": {"code": 4001, "message": "no"}},
        R("RuntimeError", "branch boom"),
        None, {}, [1, 2],
    ]:
        add("session.branch_whole", {"rid": rid, "params": {}, "session": {"cols": 40}},
            io={"branch_live": S(ret)})

    # _save_via_compute_host (6)
    saves = [
        (S({"result": {"saved": True}, "code": 0}), S(None)),
        (S({"result": [1, 2], "code": 0}), S(None)),
        (S(R("RuntimeError", "host gone")), S(None)),
        (S({"result": {"ok": 1}, "code": 5011}), S(None)),
        (S({"result": {"ok": 1}, "code": 403}), S({"jsonrpc": "2.0", "id": rid, "error": {"code": 403, "message": "denied"}})),
        (S({}), S(None)),
    ]
    for send, ackerr in saves:
        add("_save_via_compute_host", {"rid": rid, "params": {"session_id": "  s1  "}},
            io={"send_compute_host_control": send, "compute_host_ack_error": ackerr})

    # spawn_tree.load (6)
    loads = [
        ({"path": ""}, None, None),
        ({"path": "  /root/a.json  "}, R("ValueError", "path outside root"), None),
        ({"path": "/root/a.json"}, "/root/a.json", {"a": 1}),
        ({"path": "/root/b.json"}, "/root/b.json", R("OSError", "cannot read")),
        ({"path": "/root/c.json"}, "/root/c.json", [1, 2, 3]),
        ({"path": "relative.json"}, "/root/relative.json", {"z": True}),
    ]
    for params, resolve, read in loads:
        io = {}
        if resolve is not None:
            io["path_resolve"] = S(resolve)
        if read is not None:
            io["path_read_json"] = S(read)
        add("spawn_tree.load", {"rid": rid, "params": params}, io=io)

    return C


# ---------------------------------------------------------------------------
# Live drive: AST-extract each function, bind recording stubs, run it.
# ---------------------------------------------------------------------------
_src = SRC.read_text(encoding="utf-8")

# decorator RPC name -> synthetic function name, keyed by fixture func
EXTRACT = {
    "terminal.resize": (2180, "terminal_resize"),
    "delegation.status": (2073, "delegation_status"),
    "delegation.pause": (2081, "delegation_pause"),
    "session.events.stats": (2203, "events_stats"),
    "session.events.since": (2186, "events_since"),
    "pet.disable": (1567, "pet_disable"),
    "pet.scale": (1575, "pet_scale"),
    "pet.cancel": (1583, "pet_cancel"),
    "pet.generate.status": (1591, "pet_gen_status"),
    "session.close": (2054, "session_close"),
    "session.branch": (2061, "session_branch"),
    "session.branch_whole": (2066, "session_branch_whole"),
    "_save_via_compute_host": (1932, "save_via_compute_host"),
    "spawn_tree.load": (2162, "spawn_tree_load"),
    "_b64": (1530, "b64_live"),
}

# positional arg-name schemas + defaults, keyed by seam name
SCHEMA = {
    "list_active_subagents": ([], {}), "is_spawn_paused": ([], {}),
    "_get_max_spawn_depth": ([], {}), "_get_max_concurrent_children": ([], {}),
    "set_spawn_paused": (["paused"], {}),
    "replay_stats": ([], {}),
    "events_since": (["sid", "last_seen"], {}),
    "latest_seq": (["sid"], {}),
    "is_truncated": (["sid", "last_seen"], {}),
    "replay_epoch": ([], {}),
    "open_requests": (["sid"], {}),
    "set_enabled": (["enabled"], {}),
    "set_pet_scale": (["scale"], {}),
    "pet_cancel_request": (["token"], {}),
    "resolve_provider": ([], {"require_references": None}),
    "list_sprite_providers": ([], {}),
    "pop_session_by_id": (["session_id"], {}),
    "teardown_popped_session": (["session", "end_reason"], {}),
    "branch_live": (["rid", "params", "session", "omit_messages"], {"omit_messages": False}),
    "send_compute_host_control": (["session_id", "route_name", "wait"], {"route_name": "session.save", "wait": True}),
    "compute_host_ack_error": (["rid", "ack", "code", "message"], {}),
}

# custom exception classes materialized by recorders
class GenerationError(Exception):
    pass
class ManifestError(Exception):
    pass
class KeyErrorNoRepr(KeyError):
    def __str__(self):
        return self.args[0] if self.args else ""
KeyValueError = KeyErrorNoRepr
KeyErrorNoRepr.__name__ = "KeyError"

_EXC = {}
for _n in dir(__builtins__):
    _o = getattr(__builtins__, _n)
    if isinstance(_o, type) and issubclass(_o, BaseException):
        _EXC[_n] = _o
_EXC.update({
    "GenerationError": GenerationError, "ManifestError": ManifestError,
    "KeyError": KeyErrorNoRepr, "BillingError": type("BillingError", (Exception,), {}),
    "JSONDecodeError": ValueError, "ExportTooLarge": Exception, "ResumeTooLarge": Exception,
})


def make_exc(spec):
    ty = spec.get("__raise__", spec.get("type", "RuntimeError"))
    msg = spec.get("message", "")
    cls = _EXC.get(ty, type(ty, (Exception,), {}))
    return cls(msg)


def rec(ctx, name):
    pos, defaults = SCHEMA.get(name, ([], {}))

    def fn(*a, **k):
        args = {}
        for i, v in enumerate(a):
            if i < len(pos):
                args[pos[i]] = v
        args.update(k)
        for key, val in defaults.items():
            if key not in args:
                args[key] = val
        r = ctx.call(name, args)
        if is_raise(r):
            raise make_exc(r)
        return r
    return fn


class PathDouble:
    def __init__(self, p, ctx):
        self.p = p
        self.ctx = ctx

    def resolve(self):
        if self.p == "/root":   # _spawn_trees_root().resolve() -- silent anchor
            return self
        r = self.ctx.call("path_resolve", {"path": self.p})
        if is_raise(r):
            raise make_exc(r)
        return PathDouble(pystr(r), self.ctx)

    def relative_to(self, other):
        return self

    def read_text(self, encoding=None):
        r = self.ctx.call("path_read_json", {"path": self.p})
        if is_raise(r):
            raise make_exc(r)
        return json.dumps(r)

    def __str__(self):
        return self.p


def real_ok(rid, result):
    return {"jsonrpc": "2.0", "id": rid, "result": result}


def real_err(rid, code, msg, data=None):
    e = {"code": code, "message": msg}
    if data is not None:
        e["data"] = data
    return {"jsonrpc": "2.0", "id": rid, "error": e}


def real_str_param(params, key, default=""):
    return str((params or {}).get(key) or "").strip() or default


def mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def build_ns(ctx):
    class _Logger:
        def debug(self, *a, **k): return None
        def info(self, *a, **k): return None
        def warning(self, *a, **k): return None
        def exception(self, *a, **k): return None
        def error(self, *a, **k): return None
    class _NullCM:
        def __enter__(self): return self
        def __exit__(self, *a): return False
    ns = {"_ok": real_ok, "_err": real_err, "_str_param": real_str_param,
          "json": json, "Path": lambda p: PathDouble(str(p), ctx),
          "logger": _Logger(), "_session_resume_lock": _NullCM(),
          "_spawn_trees_root": lambda: PathDouble("/root", ctx)}
    # bare-global seams
    for g, s in [
        ("_pet_cancel_request", "pet_cancel_request"),
        ("_branch_live", "branch_live"),
        ("_pop_session_by_id", "pop_session_by_id"),
        ("_teardown_popped_session", "teardown_popped_session"),
        ("_send_compute_host_control", "send_compute_host_control"),
        ("_compute_host_ack_error", "compute_host_ack_error"),
        ("_open_requests", "open_requests"),
    ]:
        ns[g] = rec(ctx, s)

    # in-body import modules
    delegate = types.SimpleNamespace(
        list_active_subagents=rec(ctx, "list_active_subagents"),
        is_spawn_paused=rec(ctx, "is_spawn_paused"),
        _get_max_spawn_depth=rec(ctx, "_get_max_spawn_depth"),
        _get_max_concurrent_children=rec(ctx, "_get_max_concurrent_children"),
        set_spawn_paused=rec(ctx, "set_spawn_paused"),
    )
    tools = mod("tools", delegate_tool=delegate)
    mod("tools.delegate_tool", set_spawn_paused=rec(ctx, "set_spawn_paused"))

    er = types.SimpleNamespace(
        replay_stats=rec(ctx, "replay_stats"),
        events_since=rec(ctx, "events_since"),
        latest_seq=rec(ctx, "latest_seq"),
        is_truncated=rec(ctx, "is_truncated"),
        replay_epoch=rec(ctx, "replay_epoch"),
    )
    mod("tui_gateway", event_replay=er)

    pets = mod("hermes_cli.pets",
               _set_enabled=rec(ctx, "set_enabled"),
               set_pet_scale=rec(ctx, "set_pet_scale"))
    mod("hermes_cli")

    ig = mod("agent.pet.generate.imagegen",
             GenerationError=GenerationError,
             list_sprite_providers=rec(ctx, "list_sprite_providers"),
             resolve_provider=rec(ctx, "resolve_provider"))
    mod("agent.pet.generate")
    mod("agent.pet")
    mod("agent")
    return ns


def extract_fn(key, synth, ns):
    lineno = EXTRACT[key][0]
    node = next(n for n in ast.walk(ast.parse(_src))
                if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.lineno == lineno)
    lines = _src.splitlines()
    text = "\n".join(lines[node.lineno - 1:node.end_lineno])
    text = textwrap.dedent(text)
    # rename `def _(` -> `def <synth>(`
    idx = text.find("def _(")
    if idx >= 0:
        text = text[:idx] + "def %s(" % synth + text[idx + len("def _("):]
    else:
        # undecorated named function: rename its name too for consistency
        old = node.name
        text = text.replace("def %s(" % old, "def %s(" % synth, 1)
    exec(compile(text, "<live-%s>" % key, "exec", flags=_FLAGS, dont_inherit=True), ns)
    return ns[synth]


def drive_live(rec_case):
    key = rec_case["func"]
    inp = rec_case["in"]
    args = copy.deepcopy(inp.get("args") or {})
    ctx = Ctx(inp.get("io"), inp.get("state"))
    ns = build_ns(ctx)
    synth = EXTRACT[key][1]
    fn = extract_fn(key, synth, ns)
    # per-function argument adaptations
    if key == "_b64" and isinstance(args.get("data"), str):
        args["data"] = args["data"].encode("ascii")
    # fill in any required positional arg the fixture omitted (e.g. params)
    try:
        import inspect
        params = inspect.signature(fn).parameters
    except Exception:
        params = {}
    for pname, p in params.items():
        if pname == "session":
            continue
        if pname not in args and p.default is inspect.Parameter.empty \
                and p.kind in (inspect.Parameter.POSITIONAL_ONLY, inspect.Parameter.POSITIONAL_OR_KEYWORD):
            args[pname] = {}
    # functions whose live body mutates `session` use the state mirror
    if "session" in params and "session" not in args:
        args["session"] = ctx.state
    try:
        ret = fn(**args)
    except BaseException as e:
        ret = {"__raise__": type(e).__name__, "message": str(e)}
    return {"ret": ret, "trace": ctx.trace, "state": ctx.state}


def shadow_diff(cases):
    mism = 0
    for case in cases:
        live = drive_live(case)
        mirror = oracle.reference(case)
        if canon(live) != canon(mirror):
            mism += 1
            if mism <= 15:
                print("SHADOW MISMATCH func=%s" % case["func"], file=sys.stderr)
                print("  live  :", json.dumps(canon(live), ensure_ascii=False, sort_keys=True), file=sys.stderr)
                print("  mirror:", json.dumps(canon(mirror), ensure_ascii=False, sort_keys=True), file=sys.stderr)
    return mism


def main():
    cases = build_cases()
    check_only = "--check" in sys.argv
    if not check_only:
        FIX.parent.mkdir(parents=True, exist_ok=True)
        with open(FIX, "w", encoding="utf-8", newline="\n") as f:
            for c in cases:
                f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
        print("wrote %d fixtures -> %s" % (len(cases), FIX))
    mism = shadow_diff(cases)
    print("SHADOW_DIFF tui_methods_session_flow2: %d cases, %d mismatches" % (len(cases), mism))
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
