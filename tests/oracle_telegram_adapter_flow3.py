#!/usr/bin/env python3
"""Oracle for the Telegram adapter FLOW remainder part 3 port
(src/gateway/platforms/telegram_adapter_flow3.c).

Drives the LIVE plugins/platforms/telegram/adapter.py pure/near-pure decision
surface with SCRIPTED seams.  Claimed siblings / unported collaborators
(os.getenv, describe_proxy_error, redact_sensitive_text) are recording stubs
fed the SAME scripts the C harness feeds its universal seam.  Compares the
ordered seam trace + final return + the state echo.

The live defs are AST-extracted from the adapter module body and the
TelegramAdapter class body (decorators stripped by lineno, segments dedented);
class constants are exec'd from the class body.  Every exec namespace is a
registered sys.modules module (py3.14 annotation resolution).  The heavy
quarry import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2.  Per-case wrapper catches
BaseException (SystemExit included).  Summary:
'ORACLE telegram_adapter_flow3: N cases, M mismatches'.

Representation contract (shared with the C port): see the port header.
Notably: exception specs are {"__raise__": cls, "message": str(exc)}; the
SendResult projects to its full field dict; MessageType projects to .value;
str()-position numbers are non-integral floats in the fixture corpus.
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

# name -> "instance" (bound on the double) or "free" (module/static, unbound)
KINDS = {
    "_is_id_like": "free",
    "_caption_1024": "free",
    "_ext_from_path": "free",
    "_rows_of_two": "free",
    "_entity_type": "free",
    "_flood_cap_result": "free",
    "_is_permanent_fatal": "instance",
    "_should_drop_delayed_delivery": "instance",
    "_is_own_message": "instance",
    "_sender_is_other_bot": "instance",
    "_media_message_type": "instance",
    "_text_batch_delay_for": "instance",
    "_env_float_clamped": "free",
    "_redact_telegram_error_text": "free",
}
CLASS_CONSTS = ["_SPLIT_THRESHOLD", "_TEXT_BATCH_FAST_LEN", "_TEXT_BATCH_SHORT_LEN",
                "_TEXT_BATCH_FAST_DELAY_S", "_TEXT_BATCH_SHORT_DELAY_S"]
SEAM_NAMES = ["os.getenv", "describe_proxy_error", "redact_sensitive_text"]


# gateway.platforms.event.MessageType (verbatim values)
class MessageType(enum.Enum):
    TEXT = "text"; LOCATION = "location"; PHOTO = "photo"; VIDEO = "video"
    AUDIO = "audio"; VOICE = "voice"; DOCUMENT = "document"; STICKER = "sticker"
    COMMAND = "command"


def _send_result(success=False, message_id=None, error=None, raw_response=None,
                 retryable=False, retry_after=None, continuation_message_ids=(),
                 error_kind=None):
    """Projection of gateway.platforms.base.SendResult as a field dict."""
    return dict(success=success, message_id=message_id, error=error,
                raw_response=raw_response, retryable=retryable, retry_after=retry_after,
                continuation_message_ids=list(continuation_message_ids), error_kind=error_kind)


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
    mod = new_module("ta3_oracle_adapter")
    ns = mod.__dict__
    ns.update({"re": re, "os": None, "json": json, "logger": _SilentLogger(),
               "SendResult": _send_result, "MessageType": MessageType,
               "describe_proxy_error": None,
               "_SPLIT_THRESHOLD": None, "_TEXT_BATCH_FAST_LEN": None,
               "_TEXT_BATCH_SHORT_LEN": None, "_TEXT_BATCH_FAST_DELAY_S": None,
               "_TEXT_BATCH_SHORT_DELAY_S": None})
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "TelegramAdapter")
    # class constants
    for node in cls.body:
        if isinstance(node, ast.Assign):
            names = [t.id for t in node.targets if isinstance(t, ast.Name)]
            if any(nm in CLASS_CONSTS for nm in names):
                exec_node(src, node, ns, A_SRC)
    # functions: module-level and class-level
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
    required = list(KINDS) + CLASS_CONSTS
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_telegram_adapter_flow3: missing: {missing}")
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


# ── per-case script backend (mirrors the C tg3_call exactly) ──
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


# ── exception registry (live exception objects) ──
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
        objs[eid] = classes[eid](e.get("message", ""))
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


def describe_proxy_error_stub(error, proxy_url):
    proj = None if error is None else {"class": type(error).__name__, "message": str(error)}
    return _CUR.call("describe_proxy_error", {"error": proj, "proxy_url": proxy_url})


def make_double(state):
    clsdict = {}
    for name, kind in KINDS.items():
        if kind == "instance":
            clsdict[name] = NS[name]
    for cname in CLASS_CONSTS:
        clsdict[cname] = NS[cname]
    D = type("TelegramAdapter", (), clsdict)
    inst = D()
    for k, v in (state or {}).items():
        setattr(inst, k, mat_state(k, v))
    return D, inst


def drive(func, inp):
    global _CUR
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    _CUR = ctx
    objs = build_errors(inp.get("errors"))

    NS["os"] = SimpleNamespace(getenv=rec("os.getenv", lambda name: {"name": name}))
    NS["describe_proxy_error"] = describe_proxy_error_stub
    NS["SendResult"] = _send_result
    NS["MessageType"] = MessageType
    # in-function `from agent.redact import redact_sensitive_text` resolves sys.modules
    ag = types.ModuleType("agent"); ag.__path__ = []
    rd = types.ModuleType("agent.redact")
    rd.redact_sensitive_text = rec(
        "redact_sensitive_text", lambda text, force=False: {"text": text, "force": force})
    sys.modules["agent"] = ag
    sys.modules["agent.redact"] = rd

    D, inst = make_double(state)
    try:
        if func == "_is_id_like":
            ret = NS[func](args.get("value"))
        elif func == "_caption_1024":
            ret = NS[func](args.get("caption"))
        elif func == "_ext_from_path":
            ret = NS[func](args.get("file_path"), args.get("candidates", []), args.get("default", ""))
        elif func == "_rows_of_two":
            ret = NS[func](args.get("buttons", []))
        elif func == "_entity_type":
            ret = NS[func](ns_double(args.get("entity")))
        elif func == "_flood_cap_result":
            ret = NS[func](args.get("wait"))
        elif func == "_is_permanent_fatal":
            ret = inst._is_permanent_fatal()
        elif func == "_should_drop_delayed_delivery":
            ret = inst._should_drop_delayed_delivery()
        elif func == "_is_own_message":
            ret = inst._is_own_message(ns_double(args.get("message")))
        elif func == "_sender_is_other_bot":
            ret = inst._sender_is_other_bot(ns_double(args.get("message")))
        elif func == "_media_message_type":
            ret = inst._media_message_type(ns_double(args.get("msg")))
        elif func == "_text_batch_delay_for":
            pend = args.get("pending")
            ret = inst._text_batch_delay_for(ns_double(pend) if pend is not None else None)
        elif func == "_env_float_clamped":
            ret = NS[func](args.get("name"), args.get("default", 0.0),
                           min_value=args.get("min_value"), max_value=args.get("max_value"))
        elif func == "_redact_telegram_error_text":
            eid = args.get("error")
            err = objs.get(eid) if isinstance(eid, str) else None
            ret = NS[func](err, args.get("proxy_url"))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(vars(inst))}


# ── cross-checks (C seams / fixtures / C funcs) ──
def cross_check():
    csrc = ( _REPO / "src" / "gateway" / "platforms" / "telegram_adapter_flow3.c").read_text(encoding="utf-8")
    c_seams = set(re.findall(r'tg3_call\(\s*ctx,\s*"([^"]+)"', csrc))
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc and n not in c_seams]
    if missing_c:
        print(f"FATAL oracle_telegram_adapter_flow3: seams absent from C port: {missing_c}")
        sys.exit(2)
    extra_c = c_seams - set(SEAM_NAMES)
    if extra_c:
        print(f"FATAL oracle_telegram_adapter_flow3: C seams outside registry: {sorted(extra_c)}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'_CUR\.call\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'rec\(\s*"([^"]+)"', osrc))
    unknown = used - set(SEAM_NAMES)
    if unknown:
        print(f"FATAL oracle_telegram_adapter_flow3: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    # PoP annotation coverage
    pops = set(re.findall(r'PoP:\s*tg3_\w+\s*@\s*plugins/platforms/telegram/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_telegram_adapter_flow3: PoP/KINDS drift: c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow3.jsonl"
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
            print(f"FATAL oracle_telegram_adapter_flow3: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        unbound = fix_funcs - set(KINDS)
        if unbound:
            print(f"FATAL oracle_telegram_adapter_flow3: fixture funcs missing from KINDS: {sorted(unbound)}")
            sys.exit(2)
        missing_funcs = set(KINDS) - fix_funcs
        if missing_funcs:
            print(f"FATAL oracle_telegram_adapter_flow3: KINDS funcs missing from fixtures: {sorted(missing_funcs)}")
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
    print(f"ORACLE telegram_adapter_flow3: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow3.jsonl"
    n = bad = 0
    counts = {}
    seam_attrs = set(KINDS)
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
    print(f"SELFTEST telegram_adapter_flow3: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
