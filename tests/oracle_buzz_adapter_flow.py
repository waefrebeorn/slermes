#!/usr/bin/env python3
"""Oracle for the Buzz adapter FLOW port
(src/gateway/platforms/buzz_adapter_flow.c).

Drives the LIVE plugins/platforms/buzz/adapter.py event-gating / DM-classification
/ thread-anchoring / event-meta state cluster, the CLI text leaves, the identity
leaves and the scoped config/credentials chain with SCRIPTED seams: every SEAMED
foreign collaborator (npub_to_hex, _profile_scoped, _get_scoped_secret, os.getenv,
time.monotonic, _credentials_candidates, read_text) is a recording stub fed the
same scripts the C harness feeds its universal seam ba_call. Compares the DECISION
SEQUENCE (ordered seam events {"name","args","ret"} in "trace") plus the final
return ("ret") and the mutated state ("state").

The live defs are AST-extracted from adapter.py (module constants seeded, Assign
AND AnnAssign, plain-Name targets only; the BuzzAdapter methods extracted with
decorators stripped; _SEEN_CAP seeded from buzz_cursors.py; _THREAD_ROOT_CACHE
from the class body; logger re-set silent AFTER seeding). Every exec namespace is
a registered sys.modules module (py3.14). The heavy quarry import chain
(gateway.platforms.base / event / config, plugins.platforms.buzz.buzz_cursors,
agent.secret_scope, gateway.platforms._shared, gateway.authz_mixin) is never
imported; the imported names are seeded as placeholders / recorders.

Mode table (mirrors the C port): every ported function runs REAL (inline); only
the seven foreign collaborators are SEAMED (regex-extracted from the C ba_call
sites). A REAL function calls its REAL children directly and its SEAMED children
through the recorders.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included); scripted raises round-trip their spec bare.
Summary: 'ORACLE buzz_adapter_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- The adapter self double is a real type("BuzzAdapter", (), clsdict) instance;
  a DIRECT self._attr read with the key absent raises AttributeError
  "'BuzzAdapter' object has no attribute 'X'".
- OrderedDict-backed maps (seen / event_meta / thread_roots) materialize as
  OrderedDict; event_meta values are tuples; tuple/generator/set returns project
  to lists (set -> sorted). Integral floats == ints (canon folds).
- redact_path / credentials path doubles: {path, name} (name == basename).
- Fixture corpus restrictions are documented in the C port header.
"""
import ast
import copy
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from collections import OrderedDict
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "plugins", "platforms", "buzz", "adapter.py")
CURSORS_SRC = os.path.join(Q, "plugins", "platforms", "buzz", "buzz_cursors.py")
_FLAGS = _fut.annotations.compiler_flag

# module-level functions ported REAL
MODULE_TARGETS = [
    "_split_csv", "_add_pubkey", "_normalize_user_ref", "_pubkey_set", "_ttl_get",
    "_credentials_key", "_bounded_cli_message", "_json_or", "_cli_error_message",
    "_parse_send_receipt", "_parse_json_list",
    "_e_tags", "_event_reply_parent_id", "_p_tagged", "_scoped_platform_setting",
    "_setting_or", "_configured_relay", "_configured_home_channel",
    "_configured_credentials_file", "_reply_to_mode", "_resolve_credentials_data",
    "_resolve_private_key",
]
# BuzzAdapter methods ported REAL: name -> kind
METHOD_KINDS = {
    "normalize_user_id": "staticmethod",
    "_attachment_rejection_note": "staticmethod",
    "_extract_thread_root": "staticmethod",
    "_store_event_meta": "staticmethod",
    "_lookup_event_meta": "staticmethod",
    "_trim_seen": "staticmethod",
    "_record_thread_root": "method",
    "_resolve_reply_anchor": "method",
    "_reply_args": "method",
    "_remember_event": "method",
    "_remember_event_meta": "method",
    "_new_channel_state": "method",
    "_restore_channel_state": "method",
    "_mark_seen": "method",
    "_may_reclassify_as_dm": "method",
    "_p_tagged_to_self": "method",
    "_is_mentioned": "method",
    "_is_addressed": "method",
    "_is_direct_message_event": "method",
    "_maybe_latch_dm": "method",
    "_strip_mention": "method",
}
TARGETS = MODULE_TARGETS + list(METHOD_KINDS)
CLASS_CONSTS = ["_THREAD_ROOT_CACHE"]

# state-emission flags (mirror the harness): bit0 self, bit1 args.state, bit2 bucket
SELF_EMIT = {nm for nm, k in METHOD_KINDS.items()
             if k == "method" and nm not in ("_remember_event", "_new_channel_state")}
STATE_EMIT = {"_store_event_meta", "_trim_seen", "_remember_event", "_maybe_latch_dm"}
BUCKET_EMIT = {"_add_pubkey"}


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


class _SilentLogger:
    def __getattr__(self, level):
        def log(*a, **k):
            return None
        return log


def extract():
    mod = new_module("ba_oracle_adapter")
    ns = mod.__dict__
    import logging
    import time as _time
    ns.update({"re": re, "json": json, "os": os, "time": _time, "logging": logging,
               "OrderedDict": OrderedDict, "logger": _SilentLogger()})
    # imported names used by ported defs (seeded; recorders installed per case)
    ns["npub_to_hex"] = None
    ns["_profile_scoped"] = None
    ns["_shared_scoped_secret"] = None
    ns["Any"] = object
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    # module constants (plain-Name Assign / AnnAssign targets only)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, SRC)
            except Exception:
                pass
    # _SEEN_CAP from the sibling cursor module (plain Assign)
    csrc = open(CURSORS_SRC, encoding="utf-8").read()
    ctree = ast.parse(csrc)
    for node in ctree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "_SEEN_CAP" for t in node.targets):
            exec_node(csrc, node, ns, CURSORS_SRC)
    # module-level functions
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            exec_node(src, node, ns, SRC)
    # BuzzAdapter methods + class constants
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "BuzzAdapter":
            for sub in node.body:
                if isinstance(sub, ast.Assign) and any(
                        isinstance(t, ast.Name) and t.id in CLASS_CONSTS for t in sub.targets):
                    exec_node(src, sub, ns, SRC)
                if isinstance(sub, (ast.FunctionDef, ast.AsyncFunctionDef)) and sub.name in METHOD_KINDS:
                    clean = copy.copy(sub)
                    clean.decorator_list = []
                    exec_node(src, clean, ns, SRC)
    ns["logger"] = _SilentLogger()
    required = TARGETS + CLASS_CONSTS + [
        "_HEX64_RE", "_CHAT_KIND", "_DISPATCH_KINDS", "_KEY_FIELDS",
        "_MAX_CLI_MESSAGE_CHARS", "_EVENT_META_CONTENT_CAP", "_SEEN_CAP",
    ]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_buzz_adapter_flow: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()


# ── SEAMED names: regex-extracted from the C port's ba_call sites ──
def seam_names_from_c():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "buzz_adapter_flow.c",
                encoding="utf-8").read()
    return set(re.findall(r'ba_call\(\s*ctx,\s*"([^"]+)"', csrc))


SEAMED = seam_names_from_c()
FOREIGN_SEAMS = {"os.getenv", "time.monotonic", "read_text"}


# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, (frozenset, set)):
            return sorted(conv(i) for i in x)
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception materialization / projection ──
EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "OSError": OSError,
    "Exception": Exception, "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "StopIteration": StopIteration,
    "CancelledError": type("CancelledError", (BaseException,), {}),
    "JSONDecodeError": json.JSONDecodeError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    msg = spec.get("message", "")
    if cls is json.JSONDecodeError:
        return cls(msg, "", 0)
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C ba_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.scripted = []

    def _raise(self, name, args, spec, exc):
        self.trace.append({"name": name, "args": canon(args), "ret": spec})
        self.scripted.append((exc, spec))
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            msg = f"no script for {name}"
            self._raise(name, args, {"__raise__": "RuntimeError", "message": msg}, RuntimeError(msg))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                msg = f"script exhausted for {name}"
                self._raise(name, args, {"__raise__": "RuntimeError", "message": msg}, RuntimeError(msg))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                rs = used["raise"]
                spec = {"__raise__": rs.get("type", "RuntimeError"), "message": rs.get("message", "")}
                self._raise(name, args, spec, make_exc(rs))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return ret


# ── path doubles ──
class RedactPath:
    def __init__(self, path, name):
        self._path = path
        self.name = name

    def __str__(self):
        return self._path


class CredPath:
    def __init__(self, path, ctx):
        self.path = path
        self._ctx = ctx

    def read_text(self, encoding="utf-8"):
        return self._ctx.call("read_text", {"path": self.path, "encoding": encoding})


def mat_redact(d):
    if d is None:
        return None
    return RedactPath(d["path"], d["name"])


def mat_config(d):
    if d is None:
        return None
    return types.SimpleNamespace(**d)


def mat_state(sv):
    out = {}
    if "chat_type" in sv:
        out["chat_type"] = sv["chat_type"]
    if "last_ts" in sv:
        out["last_ts"] = sv["last_ts"]
    if "seen" in sv:
        out["seen"] = OrderedDict((k, None) for k in sv["seen"])
    if "event_meta" in sv:
        out["event_meta"] = OrderedDict(
            (k, tuple(x) if isinstance(x, list) else x) for k, x in sv["event_meta"].items())
    return out


def mat_self_attr(k, v):
    if k == "_channel_state" and isinstance(v, dict):
        return {cid: mat_state(sv) for cid, sv in v.items()}
    if k == "_thread_roots" and isinstance(v, dict):
        return OrderedDict(v.items())
    return v


# ── seam argument projections / result builders ──
ARGMAP = {
    "npub_to_hex": lambda ref: {"ref": ref},
    "_profile_scoped": lambda: {},
    "_get_scoped_secret": lambda name, default=None: {"name": name, "default": default},
    "os.getenv": lambda name: {"name": name},
    "time.monotonic": lambda: {},
    "_credentials_candidates": lambda extra=None: {"extra": extra},
    "read_text": lambda self, encoding="utf-8": {"path": self.path, "encoding": encoding},
}
BUILD = {
    "_credentials_candidates": lambda r, ctx: [CredPath(p["path"], ctx) for p in r],
}


def make_recorder(ctx, nm):
    argfn = ARGMAP[nm]
    build = BUILD.get(nm)

    def f(*a, **k):
        raw = ctx.call(nm, argfn(*a, **k))
        return build(raw, ctx) if build else raw
    return f


def install_recorders(ctx, target):
    for nm in SEAMED:
        if nm == "os.getenv":
            NS["os"] = types.SimpleNamespace(getenv=make_recorder(ctx, nm))
        elif nm == "time.monotonic":
            NS["time"] = types.SimpleNamespace(monotonic=make_recorder(ctx, nm))
        elif nm == "read_text":
            continue  # bound on CredPath doubles
        elif nm in NS and nm != target:
            NS[nm] = make_recorder(ctx, nm)


# ── adapter double ──
def make_double(state):
    clsdict = {}
    for name, kind in METHOD_KINDS.items():
        fn = NS[name]
        clsdict[name] = staticmethod(fn) if kind == "staticmethod" else fn
    for cname in CLASS_CONSTS:
        clsdict[cname] = NS[cname]
    D = type("BuzzAdapter", (), clsdict)
    inst = D()
    for k, v in (state or {}).items():
        setattr(inst, k, mat_self_attr(k, copy.deepcopy(v)))
    return D, inst


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


# ── per-case live call ──
def live_call(func, args, D, inst, state_out):
    if func in MODULE_TARGETS:
        f = NS[func]
        if func == "_split_csv":
            return f(args["raw"])
        if func == "_add_pubkey":
            bucket = args["bucket"]
            state_out["bucket"] = bucket
            return f(bucket, args["raw"])
        if func == "_normalize_user_ref":
            return f(args["ref"])
        if func == "_pubkey_set":
            return f(args["raw"])
        if func == "_ttl_get":
            return f(args["cache"], args["key"], args["ttl"])
        if func == "_credentials_key":
            return f(args["data"])
        if func == "_bounded_cli_message":
            return f(args["message"], redact_path=mat_redact(args.get("redact_path")))
        if func == "_json_or":
            return f(args["text"], args["default"])
        if func == "_cli_error_message":
            return f(args["stderr"], args["returncode"], redact_path=mat_redact(args.get("redact_path")))
        if func == "_parse_send_receipt":
            return f(args["stdout"])
        if func == "_parse_json_list":
            return f(args["stdout"])
        if func == "_e_tags":
            return list(f(args["event"]))
        if func == "_event_reply_parent_id":
            return f(args["event"])
        if func == "_p_tagged":
            return f(args["event"], args["pubkey"])
        if func == "_scoped_platform_setting":
            return f(args["env_name"], args["extra"], args["key"])
        if func == "_setting_or":
            return f(args["env_name"], args["extra"], args["key"], args["default"])
        if func == "_configured_relay":
            return f(args["extra"])
        if func == "_configured_home_channel":
            return f(args["extra"])
        if func == "_configured_credentials_file":
            return f(args["extra"])
        if func == "_reply_to_mode":
            return f(mat_config(args.get("config")), args["extra"])
        if func == "_resolve_credentials_data":
            return f(args["extra"])
        if func == "_resolve_private_key":
            return f(args["extra"])
    # methods
    if func == "normalize_user_id":
        return D.normalize_user_id(args["user_id"])
    if func == "_attachment_rejection_note":
        return D._attachment_rejection_note(args["rejected"])
    if func == "_extract_thread_root":
        return D._extract_thread_root(args["event"])
    if func == "_store_event_meta":
        st = mat_state(args["state"]); state_out["state"] = st
        return D._store_event_meta(st, args["event_id"], args["pubkey"], args["snippet"])
    if func == "_lookup_event_meta":
        return D._lookup_event_meta(mat_state(args["state"]), args["event_id"])
    if func == "_trim_seen":
        st = mat_state(args["state"]); state_out["state"] = st
        return D._trim_seen(st)
    if func == "_remember_event":
        st = mat_state(args["state"]); state_out["state"] = st
        return inst._remember_event(st, args["event"])
    if func == "_remember_event_meta":
        return inst._remember_event_meta(args["channel_id"], args["event_id"], args["pubkey"], args["content"])
    if func == "_new_channel_state":
        return inst._new_channel_state(args["chat_type"])
    if func == "_restore_channel_state":
        return inst._restore_channel_state(args["channel_id"], args["chat_type"])
    if func == "_mark_seen":
        return inst._mark_seen(args["channel_id"], args["event_id"])
    if func == "_record_thread_root":
        return inst._record_thread_root(args["event_id"], args["event"])
    if func == "_resolve_reply_anchor":
        return inst._resolve_reply_anchor(args["anchor"])
    if func == "_reply_args":
        return inst._reply_args(args["anchor"])
    if func == "_may_reclassify_as_dm":
        return inst._may_reclassify_as_dm(args["channel_id"])
    if func == "_p_tagged_to_self":
        return inst._p_tagged_to_self(args["event"])
    if func == "_is_mentioned":
        return inst._is_mentioned(args["content"])
    if func == "_is_addressed":
        return inst._is_addressed(args["event"])
    if func == "_is_direct_message_event":
        return inst._is_direct_message_event(args["channel_id"], args["event"])
    if func == "_maybe_latch_dm":
        st = mat_state(args["state"]); state_out["state"] = st
        return inst._maybe_latch_dm(args["channel_id"], st, args["event"])
    if func == "_strip_mention":
        return inst._strip_mention(args["content"])
    raise RuntimeError(f"unknown func {func}")


# ── per-case drive ──
def drive(func, inp):
    args = copy.deepcopy(inp.get("args") or {})
    ctx = Ctx(inp.get("io"))
    install_recorders(ctx, func)
    D, inst = make_double(inp.get("state"))
    state_out = {}
    try:
        ret = live_call(func, args, D, inst, state_out)
    except BaseException as exc:  # SystemExit included
        ret = exc_spec(exc, ctx)
    if func in SELF_EMIT:
        state_out["self"] = vars(inst)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check ──
def cross_check():
    missing_argmap = sorted(SEAMED - set(ARGMAP))
    if missing_argmap:
        print(f"FATAL oracle_buzz_adapter_flow: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra_argmap = sorted(set(ARGMAP) - SEAMED)
    if extra_argmap:
        print(f"FATAL oracle_buzz_adapter_flow: ARGMAP seams absent from C: {extra_argmap}")
        sys.exit(2)
    missing_def = sorted(nm for nm in SEAMED if nm not in FOREIGN_SEAMS and nm not in NS)
    if missing_def:
        print(f"FATAL oracle_buzz_adapter_flow: seams without live def: {missing_def}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "buzz_adapter_flow.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func"))
        bad = fix_keys - SEAMED
        if bad:
            print(f"FATAL oracle_buzz_adapter_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        badf = fix_funcs - set(TARGETS)
        if badf:
            print(f"FATAL oracle_buzz_adapter_flow: fixture funcs without a target: {sorted(badf)}")
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
        for k in sorted(keys)[:3]:
            print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:200]}")
            print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:200]}")


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
    print(f"ORACLE buzz_adapter_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(60, exit=True)
    fix = _REPO / "tests" / "fixtures" / "buzz_adapter_flow.jsonl"
    n = bad = 0
    counts = {}
    extraction_names = set(TARGETS) | SEAMED
    for line in open(fix, encoding="utf-8"):
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
        io = (case.get("in") or {}).get("io") or {}
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            # an entirely empty io is a deliberate fail-closed case; a partial
            # script that gaps on a called seam is the binding-bug signal.
            if io:
                bad += 1
                print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        # flag AttributeError specs naming a ported method / seam stub (uniform-family trap)
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            for nm in extraction_names:
                if f"'{nm}'" in msg and nm not in ("_self_pubkey", "_self_npub", "_display_name",
                                                    "_channel_meta", "_channel_state", "_channel_names",
                                                    "_thread_roots", "_restored_cursors", "_reply_to_mode"):
                    bad += 1
                    print(f"SELFTEST ATTR FAMILY {func} case {n}: {msg}")
                    break
    faulthandler.cancel_dump_traceback_later()
    print(f"SELFTEST buzz_adapter_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    missing = set(TARGETS) - set(counts)
    if missing:
        print(f"SELFTEST MISSING FUNCS: {sorted(missing)}")
        bad += 1
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
