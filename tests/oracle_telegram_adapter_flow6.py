#!/usr/bin/env python3
"""Oracle for the Telegram adapter FLOW part 6 port
(src/gateway/platforms/telegram_adapter_flow6.c).

Drives the LIVE plugins/platforms/telegram/adapter.py inbound update
normalization / entity-mention addressing / error-classification decision
surface with SCRIPTED seams.  Claimed siblings / unported collaborators are
recording stubs fed the SAME scripts the C harness feeds its universal seam.
Compares the ordered seam trace + final return + the state echo.

The live defs are AST-extracted from the adapter module body and the
TelegramAdapter class body (decorators stripped by lineno, segments dedented);
nested defs travel with their enclosing function.  Every exec namespace is a
registered sys.modules module (py3.14 annotation resolution).  The heavy
quarry import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2.  Per-case wrapper catches
BaseException (SystemExit included).  Summary:
'ORACLE telegram_adapter_flow6: N cases, M mismatches'.

Representation contract (shared with the C port): see the port header.
Notably: exception specs are {"__raise__": cls, "message": str(exc)}; ids are
integral ints; set returns are sorted; UTF-16 slicing is code-unit aware.
"""
import ast
import asyncio
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

# name -> "instance" (bound-call style, self passed explicitly) or "free"
# (module/static function, or classmethod whose cls is passed explicitly)
KINDS = {
    "_normalize_platform_event": "instance",
    "_normalize_reaction_event": "instance",
    "_normalize_message_edited_event": "instance",
    "_telegram_entity_text": "free",
    "_entity_span": "free",
    "_chat_id_str": "free",
    "_is_group_chat": "instance",
    "_extract_bot_mention_usernames": "free",
    "_is_guest_mention": "instance",
    "_bot_sender_suppressed": "instance",
    "_clean_bot_trigger_text": "instance",
    "_current_bot_username": "instance",
    "_is_transient_typing_error": "free",
    "_dm_topic_missing_anchor_error": "free",
    "_telegram_require_mention": "instance",
    "_telegram_bots_require_mention": "instance",
    "_telegram_exclusive_bot_mentions": "instance",
}
# EXACTLY the C ta6_call seam names (set-equality is enforced in cross_check).
SEAM_NAMES = [
    "_normalize_reaction_event", "_normalize_message_edited_event", "_is_id_like",
    "isoformat", "_telegram_entity_text", "_chat_type_str", "_entity_sources",
    "_entity_type", "_entity_span", "_telegram_guest_mode", "_message_mentions_bot",
    "_telegram_bots_require_mention", "_sender_is_other_bot",
    "clean_bot_trigger_text", "_current_bot_username", "_extra_bool",
]
# seams bound as self/cls attributes (recorded on the doubles)
SELF_SEAMS = [
    "_normalize_reaction_event", "_normalize_message_edited_event", "_is_id_like",
    "_chat_type_str", "_telegram_guest_mode", "_message_mentions_bot",
    "_telegram_bots_require_mention", "_sender_is_other_bot",
    "_current_bot_username", "_extra_bool",
]
CLS_SEAMS = ["_entity_sources", "_entity_type", "_entity_span", "_telegram_entity_text"]


def seg(src, node):
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


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
    mod = new_module("ta6_oracle_adapter")
    ns = mod.__dict__
    ns.update({"re": re, "os": None, "json": json, "asyncio": asyncio,
               "logger": _SilentLogger(), "_FOREIGN_BOT_HANDLE_RE": None,
               "clean_bot_trigger_text": None})
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "TelegramAdapter")
    for node in cls.body:
        if isinstance(node, ast.Assign):
            names = [t.id for t in node.targets if isinstance(t, ast.Name)]
            if "_FOREIGN_BOT_HANDLE_RE" in names:
                exec_node(src, node, ns, A_SRC)
    found = set()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in KINDS:
            exec_node(src, node, ns, A_SRC)
            found.add(node.name)
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in KINDS:
            exec_node(src, node, ns, A_SRC)
            found.add(node.name)
    ns["logger"] = _SilentLogger()
    required = list(KINDS) + ["_FOREIGN_BOT_HANDLE_RE"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_telegram_adapter_flow6: missing: {missing}")
        sys.exit(2)
    return ns, found


NS, _FOUND = extract()


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


# ── per-case script backend (mirrors the C ta6_call exactly) ──
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


# ── exception registry (live exception objects with attributes) ──
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
            if k not in ("class", "message", "base"):
                setattr(obj, k, v)
        objs[eid] = obj
    return objs


def ns_double(v):
    if isinstance(v, dict):
        return SimpleNamespace(**{k: ns_double(x) for k, x in v.items()})
    if isinstance(v, list):
        return [ns_double(x) for x in v]
    return v


def bind_iso(obj):
    """Bind edit_date.isoformat recorders wherever an 'isoformat' attr exists."""
    if isinstance(obj, SimpleNamespace):
        for k in list(vars(obj)):
            if k == "isoformat":
                setattr(obj, k, rec("isoformat", lambda: {}))
            else:
                bind_iso(getattr(obj, k))
    elif isinstance(obj, list):
        for x in obj:
            bind_iso(x)
    return obj


def mat_state(k, v):
    if k == "_bot" and isinstance(v, dict):
        return SimpleNamespace(**v)
    return v


# ── seam stubs (explicit literal names; cross_check scans these) ──
def build_seams():
    st = {}
    st["_normalize_reaction_event"] = rec("_normalize_reaction_event", lambda update: {"update": update})
    st["_normalize_message_edited_event"] = rec("_normalize_message_edited_event", lambda update: {"update": update})
    st["_is_id_like"] = rec("_is_id_like", lambda value: {"value": value})
    st["isoformat"] = rec("isoformat", lambda: {})
    st["_telegram_entity_text"] = rec("_telegram_entity_text",
        lambda source_text, offset, length: {"source_text": source_text, "offset": offset, "length": length})
    st["_chat_type_str"] = rec("_chat_type_str", lambda chat: {"chat": chat})
    st["_entity_sources"] = rec("_entity_sources", lambda message: {"message": message})
    st["_entity_type"] = rec("_entity_type", lambda entity: {"entity": entity})
    st["_entity_span"] = rec("_entity_span", lambda source_text, entity: {"source_text": source_text, "entity": entity})
    st["_telegram_guest_mode"] = rec("_telegram_guest_mode", lambda: {})
    st["_message_mentions_bot"] = rec("_message_mentions_bot", lambda message: {"message": message})
    st["_telegram_bots_require_mention"] = rec("_telegram_bots_require_mention", lambda: {})
    st["_sender_is_other_bot"] = rec("_sender_is_other_bot", lambda message: {"message": message})
    st["clean_bot_trigger_text"] = rec("clean_bot_trigger_text", lambda text, username: {"text": text, "username": username})
    st["_current_bot_username"] = rec("_current_bot_username", lambda: {})
    st["_extra_bool"] = rec("_extra_bool", lambda key, env, default: {"key": key, "env": env, "default": default})
    return st


def make_doubles(state, stubs):
    """Return (self_double, cls_double).  Seam recorders live on the doubles;
    the ported functions themselves are always called from NS directly."""
    clsdict = {}
    for name in SELF_SEAMS:
        # staticmethod prevents Python from binding `self` into the recorder's *args
        clsdict[name] = staticmethod(stubs[name])
    D = type("TelegramAdapter", (), clsdict)
    inst = D()
    for k, v in (state or {}).items():
        setattr(inst, k, mat_state(k, v))
    cls_dict = {}
    for name in CLS_SEAMS:
        cls_dict[name] = stubs[name]
    cls_dict["_FOREIGN_BOT_HANDLE_RE"] = NS["_FOREIGN_BOT_HANDLE_RE"]
    cls = type("TelegramAdapterCls", (), cls_dict)
    return inst, cls


def drive(func, inp):
    global _CUR
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    _CUR = ctx
    objs = build_errors(inp.get("errors"))
    stubs = build_seams()

    # in-function import: clean_bot_trigger_text resolves sys.modules
    for n in ("plugins", "plugins.platforms", "plugins.platforms.telegram"):
        m = types.ModuleType(n)
        m.__path__ = []
        sys.modules[n] = m
    addr = types.ModuleType("plugins.platforms.telegram.telegram_addressing")
    addr.clean_bot_trigger_text = stubs["clean_bot_trigger_text"]
    sys.modules["plugins.platforms.telegram.telegram_addressing"] = addr

    NS["clean_bot_trigger_text"] = stubs["clean_bot_trigger_text"]
    inst, cls = make_doubles(state, stubs)

    def A(k):
        return bind_iso(ns_double(args.get(k)))

    try:
        if func == "_normalize_platform_event":
            ret = NS[func](inst, A("update"))
        elif func == "_normalize_reaction_event":
            ret = NS[func](inst, A("update"))
        elif func == "_normalize_message_edited_event":
            ret = NS[func](inst, A("update"))
        elif func == "_telegram_entity_text":
            ret = NS[func](A("source_text"), args.get("offset"), args.get("length"))
        elif func == "_entity_span":
            ret = NS[func](cls, A("source_text"), A("entity"))
        elif func == "_chat_id_str":
            ret = NS[func](A("message"))
        elif func == "_is_group_chat":
            ret = NS[func](inst, A("message"))
        elif func == "_extract_bot_mention_usernames":
            ret = NS[func](cls, A("message"), args.get("self_username", ""))
        elif func == "_is_guest_mention":
            ret = NS[func](inst, A("message"))
        elif func == "_bot_sender_suppressed":
            ret = NS[func](inst, A("message"))
        elif func == "_clean_bot_trigger_text":
            ret = NS[func](inst, A("text"))
        elif func == "_current_bot_username":
            ret = NS[func](inst)
        elif func == "_is_transient_typing_error":
            eid = args.get("exc")
            ret = NS[func](objs.get(eid) if isinstance(eid, str) else None)
        elif func == "_dm_topic_missing_anchor_error":
            ret = NS[func]()
        elif func in ("_telegram_require_mention", "_telegram_bots_require_mention",
                      "_telegram_exclusive_bot_mentions"):
            ret = NS[func](inst)
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(vars(inst))}


# ── cross-checks (C seams / fixtures / C funcs / PoP) ──
def cross_check():
    csrc = (_REPO / "src" / "gateway" / "platforms" / "telegram_adapter_flow6.c").read_text(encoding="utf-8")
    c_seams = set(re.findall(r'ta6_call\(\s*ctx,\s*"([^"]+)"', csrc))
    if c_seams != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow6: C seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - c_seams)} extra={sorted(c_seams - set(SEAM_NAMES))}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'_CUR\.call\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'rec\(\s*"([^"]+)"', osrc))
    if used != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow6: oracle seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - used)} extra={sorted(used - set(SEAM_NAMES))}")
        sys.exit(2)
    pops = set(re.findall(r'PoP:\s*ta6_\w+\s*@\s*plugins/platforms/telegram/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_telegram_adapter_flow6: PoP/KINDS drift: c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow6.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in fix.open(encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func", ""))
        bad = fix_keys - set(SEAM_NAMES)
        if bad:
            print(f"FATAL oracle_telegram_adapter_flow6: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        unbound = fix_funcs - set(KINDS)
        if unbound:
            print(f"FATAL oracle_telegram_adapter_flow6: fixture funcs missing from KINDS: {sorted(unbound)}")
            sys.exit(2)
        missing_funcs = set(KINDS) - fix_funcs
        if missing_funcs:
            print(f"FATAL oracle_telegram_adapter_flow6: KINDS funcs missing from fixtures: {sorted(missing_funcs)}")
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
    print(f"ORACLE telegram_adapter_flow6: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow6.jsonl"
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
    # KINDS must match the extraction map (a name absent from KINDS is never extracted)
    if set(counts) != set(KINDS):
        print(f"SELFTEST KINDS DRIFT: fixture funcs={sorted(counts)} kinds={sorted(KINDS)}")
        bad += 1
    print(f"SELFTEST telegram_adapter_flow6: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
