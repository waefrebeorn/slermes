#!/usr/bin/env python3
"""Oracle for the Telegram adapter FLOW part 8 port
(src/gateway/platforms/telegram_adapter_flow8.c).

Drives the LIVE plugins/platforms/telegram/adapter.py group-mention-gating /
observe-decision surface with SCRIPTED seams.  Claimed siblings / unported
collaborators are recording stubs fed the SAME scripts the C harness feeds its
universal seam.  Compares the ordered seam trace + final return + state echo.

The live defs are AST-extracted from the TelegramAdapter class body (decorators
excluded by lineno, segments dedented); the exec namespace is a registered
sys.modules module (py3.14 annotation resolution).  ``from __future__ import
annotations`` keeps signature annotations unevaluated.

Fail-closed: missing defs or 0 cases => exit 2.  Per-case wrapper catches
BaseException (SystemExit included).  Summary:
'ORACLE telegram_adapter_flow8: N cases, M mismatches'.
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

# name -> "instance" (self passed explicitly) or "cls" (classmethod cls passed).
KINDS = {
    "_telegram_observe_unmentioned_group_messages": "instance",
    "_telegram_guest_mode": "instance",
    "_telegram_free_response_chats": "instance",
    "_telegram_free_response_topics": "instance",
    "_telegram_allowed_chats": "instance",
    "_telegram_group_allowed_chats": "instance",
    "_telegram_allowed_topics": "instance",
    "_reactions_enabled": "instance",
    "_telegram_auth_env_configured": "instance",
    "_topic_id_or_general": "cls",
    "_telegram_group_observe_attributed_text": "instance",
    "_telegram_group_observe_shared_source": "instance",
    "_apply_telegram_group_observe_attribution": "instance",
    "_telegram_group_observe_channel_prompt": "instance",
    "_message_matches_mention_patterns": "instance",
    "_compile_mention_patterns": "instance",
    "_gate_or_observe": "instance",
    "_should_observe_unmentioned_group_message": "instance",
}
# EXACTLY the C ta8_call seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    "_extra_bool", "_extra_str_set", "_extra_or_secret", "_scoped_gate_env",
    "tag_safe_display_name", "dataclasses.replace", "apply_group_observe_attribution",
    "_current_bot_username", "addressing_text", "pattern.search", "json.loads",
    "compile_mention_patterns", "_should_process_message",
    "_observe_unmentioned_group_message", "_is_own_message",
    "_telegram_observe_unmentioned_group_messages", "_is_group_chat",
    "_topic_gates_pass", "_chat_id_str", "_telegram_exclusive_bot_mentions",
    "_explicit_bot_mentions_exclude_self", "_telegram_observe_allowed_chats",
    "_bot_sender_suppressed", "_telegram_free_response_chats",
    "_telegram_is_free_response_topic", "_telegram_require_mention",
    "_is_reply_to_bot", "_message_mentions_bot", "_message_matches_mention_patterns",
    "_should_observe_unmentioned_group_message",
]


class Pat:
    """Stand-in for a compiled re.Pattern: carries its scripted value and a
    recording .search method.  canon() projects it back to the value."""
    __slots__ = ("value", "_search")

    def __init__(self, value, search_stub):
        self.value = value
        self._search = search_stub

    def search(self, text):
        return self._search(self.value, text)


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
    mod = new_module("ta8_oracle_adapter")
    ns = mod.__dict__
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "TelegramAdapter")
    found = set()
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in KINDS:
            exec_node(src, node, ns, A_SRC)
            found.add(node.name)
    missing = [k for k in KINDS if k not in ns]
    if missing:
        print(f"FATAL oracle_telegram_adapter_flow8: missing: {missing}")
        sys.exit(2)
    return ns, found


NS, _FOUND = extract()


# ── canonical projection ──
def canon(v):
    def conv(x):
        if isinstance(x, Pat):
            return conv(x.value)
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


# ── per-case script backend (mirrors the C ta8_call exactly) ──
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


def rec(name, args_fn):
    def f(*a, **k):
        args = args_fn(*a, **k)
        return _CUR.call(name, args)
    return f


# ── seam stubs (explicit literal names; cross_check scans the rec calls) ──
def build_seams():
    st = {}
    st["_extra_bool"] = rec("_extra_bool",
        lambda key, env_name, default, *fallback_keys: {
            "key": key, "env_name": env_name, "default": default,
            "fallback_keys": list(fallback_keys)})
    st["_extra_str_set"] = rec("_extra_str_set",
        lambda key, env_name: {"key": key, "env_name": env_name})
    st["_extra_or_secret"] = rec("_extra_or_secret",
        lambda extra, key, env_name, default: {
            "extra": extra, "key": key, "env_name": env_name, "default": default})
    st["_scoped_gate_env"] = rec("_scoped_gate_env",
        lambda key, default="": {"key": key, "default": default})
    st["tag_safe_display_name"] = rec("tag_safe_display_name",
        lambda value: {"value": value})
    st["dataclasses.replace"] = rec("dataclasses.replace",
        lambda source, user_id, user_name, user_id_alt, shared_session: {
            "source": source, "user_id": user_id, "user_name": user_name,
            "user_id_alt": user_id_alt, "shared_session": shared_session})
    st["apply_group_observe_attribution"] = rec("apply_group_observe_attribution",
        lambda adapter, event: {"event": event})
    st["_current_bot_username"] = rec("_current_bot_username", lambda: {})
    st["addressing_text"] = rec("addressing_text", lambda message: {"message": message})
    st["pattern.search"] = rec("pattern.search",
        lambda pattern, text: {"pattern": pattern, "text": text})
    st["json.loads"] = rec("json.loads", lambda s: {"s": s})
    st["compile_mention_patterns"] = rec("compile_mention_patterns",
        lambda patterns, log_prefix, platform_label, display_label, logger_=None: {
            "patterns": patterns, "log_prefix": log_prefix,
            "platform_label": platform_label, "display_label": display_label})
    st["_should_process_message"] = rec("_should_process_message", lambda msg: {"msg": msg})
    st["_observe_unmentioned_group_message"] = rec("_observe_unmentioned_group_message",
        lambda msg, msg_type, update_id=None: {
            "msg": msg, "msg_type": msg_type, "update_id": update_id})
    st["_is_own_message"] = rec("_is_own_message", lambda message: {"message": message})
    st["_telegram_observe_unmentioned_group_messages"] = rec(
        "_telegram_observe_unmentioned_group_messages", lambda: {})
    st["_is_group_chat"] = rec("_is_group_chat", lambda message: {"message": message})
    st["_topic_gates_pass"] = rec("_topic_gates_pass",
        lambda thread_id, warn_non_numeric=True, message=None: {
            "thread_id": thread_id, "warn_non_numeric": warn_non_numeric, "message": message})
    st["_chat_id_str"] = rec("_chat_id_str", lambda message: {"message": message})
    st["_telegram_exclusive_bot_mentions"] = rec(
        "_telegram_exclusive_bot_mentions", lambda: {})
    st["_explicit_bot_mentions_exclude_self"] = rec(
        "_explicit_bot_mentions_exclude_self",
        lambda message, log_skip=True: {"message": message, "log_skip": log_skip})
    st["_telegram_observe_allowed_chats"] = rec(
        "_telegram_observe_allowed_chats", lambda: {})
    st["_bot_sender_suppressed"] = rec("_bot_sender_suppressed",
        lambda message: {"message": message})
    st["_telegram_free_response_chats"] = rec(
        "_telegram_free_response_chats", lambda: {})
    st["_telegram_is_free_response_topic"] = rec(
        "_telegram_is_free_response_topic", lambda message: {"message": message})
    st["_telegram_require_mention"] = rec("_telegram_require_mention", lambda: {})
    st["_is_reply_to_bot"] = rec("_is_reply_to_bot", lambda message: {"message": message})
    st["_message_mentions_bot"] = rec("_message_mentions_bot",
        lambda message: {"message": message})
    st["_message_matches_mention_patterns"] = rec(
        "_message_matches_mention_patterns", lambda message: {"message": message})
    st["_should_observe_unmentioned_group_message"] = rec(
        "_should_observe_unmentioned_group_message", lambda msg: {"msg": msg})
    return st


def ns_double(v):
    if isinstance(v, dict):
        return SimpleNamespace(**{k: ns_double(x) for k, x in v.items()})
    if isinstance(v, list):
        return [ns_double(x) for x in v]
    return v


def mat_state(k, v, stubs):
    if k == "config" and isinstance(v, dict):
        extra = v.get("extra") or {}
        return SimpleNamespace(extra=extra)
    if k == "_bot" and isinstance(v, dict):
        return SimpleNamespace(**v)
    if k == "_mention_patterns" and isinstance(v, list):
        return [Pat(x, stubs["pattern.search"]) for x in v]
    return v


def make_doubles(state, stubs):
    inst_attrs = {name: staticmethod(stubs[name]) for name in SEAM_NAMES}
    D = type("TelegramAdapter", (), inst_attrs)
    inst = D()
    for k, v in (state or {}).items():
        setattr(inst, k, mat_state(k, v, stubs))
    cls_attrs = {name: staticmethod(stubs[name]) for name in SEAM_NAMES}
    cls = type("TelegramAdapterCls", (), cls_attrs)
    cls._GENERAL_TOPIC_THREAD_ID = "1"
    return inst, cls


def drive(func, inp):
    global _CUR
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    _CUR = ctx
    stubs = build_seams()

    # Module globals the extracted defs resolve at call time.
    NS["json"] = SimpleNamespace(loads=stubs["json.loads"])
    NS["compile_mention_patterns"] = stubs["compile_mention_patterns"]
    NS["_scoped_gate_env"] = stubs["_scoped_gate_env"]
    NS["_extra_or_secret"] = stubs["_extra_or_secret"]
    NS["tag_safe_display_name"] = stubs["tag_safe_display_name"]
    NS["addressing_text"] = stubs["addressing_text"]
    NS["apply_group_observe_attribution"] = stubs["apply_group_observe_attribution"]
    NS["dataclasses"] = SimpleNamespace(replace=stubs["dataclasses.replace"])
    NS["logger"] = logging_stub()

    inst, cls = make_doubles(state, stubs)

    def A(k):
        return ns_double(args.get(k))

    try:
        if func in ("_telegram_observe_unmentioned_group_messages", "_telegram_guest_mode",
                    "_telegram_free_response_chats", "_telegram_free_response_topics",
                    "_telegram_allowed_chats", "_telegram_group_allowed_chats",
                    "_telegram_allowed_topics", "_reactions_enabled",
                    "_telegram_auth_env_configured", "_telegram_group_observe_channel_prompt",
                    "_compile_mention_patterns"):
            ret = NS[func](inst)
        elif func == "_topic_id_or_general":
            ret = NS[func](cls, args.get("thread_id"))
        elif func == "_telegram_group_observe_attributed_text":
            ret = NS[func](inst, A("event"))
        elif func == "_telegram_group_observe_shared_source":
            ret = NS[func](inst, A("source"))
        elif func == "_apply_telegram_group_observe_attribution":
            ret = NS[func](inst, A("event"))
        elif func == "_message_matches_mention_patterns":
            ret = NS[func](inst, A("message"))
        elif func == "_gate_or_observe":
            ret = NS[func](inst, A("msg"), A("update"), args.get("msg_type"))
        elif func == "_should_observe_unmentioned_group_message":
            ret = NS[func](inst, A("message"))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(vars(inst))}


def logging_stub():
    class _L:
        def __getattr__(self, _name):
            return lambda *a, **k: None
    return _L()


# ── cross-checks (C seams / fixtures / C funcs / PoP) ──
def cross_check():
    csrc = (_REPO / "src" / "gateway" / "platforms" /
            "telegram_adapter_flow8.c").read_text(encoding="utf-8")
    c_seams = set(re.findall(r'ta8_call\(\s*ctx,\s*"([^"]+)"', csrc))
    if c_seams != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow8: C seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - c_seams)} extra={sorted(c_seams - set(SEAM_NAMES))}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'rec\(\s*"([^"]+)"', osrc))
    if used != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow8: oracle seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - used)} extra={sorted(used - set(SEAM_NAMES))}")
        sys.exit(2)
    pops = set(re.findall(r'PoP:\s*ta8_\w+\s*@\s*plugins/platforms/telegram/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_telegram_adapter_flow8: PoP/KINDS drift: c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow8.jsonl"
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
            print(f"FATAL oracle_telegram_adapter_flow8: fixture io key set != SEAM_NAMES: "
                  f"missing={sorted(set(SEAM_NAMES) - fix_keys)} extra={sorted(fix_keys - set(SEAM_NAMES))}")
            sys.exit(2)
        if fix_funcs != set(KINDS):
            print(f"FATAL oracle_telegram_adapter_flow8: fixture func set != KINDS: "
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
    print(f"ORACLE telegram_adapter_flow8: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow8.jsonl"
    n = bad = 0
    counts = {}
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
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            if not (case.get("in") or {}).get("expect_fail_closed"):
                bad += 1
                print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
    if set(counts) != set(KINDS):
        print(f"SELFTEST KINDS DRIFT: fixture funcs={sorted(counts)} kinds={sorted(KINDS)}")
        bad += 1
    print(f"SELFTEST telegram_adapter_flow8: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
