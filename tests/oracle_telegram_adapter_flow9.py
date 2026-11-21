#!/usr/bin/env python3
"""Oracle for the Telegram adapter FLOW part 9 port
(src/gateway/platforms/telegram_adapter_flow9.c).

Drives the LIVE plugins/platforms/telegram/adapter.py formatting / small-helper
surface with SCRIPTED seams.  Every non-pure collaborator (``t``,
``html.escape``, ``InlineKeyboardButton``, ``TelegramAdapter``,
``resolve_notifications_mode``, ``LinkPreviewOptions``,
``parse_fallback_ip_env``) is a recording stub fed the SAME scripts the C
harness feeds its universal seam.  Compares the ordered seam trace + final
return + state echo.

The live defs are AST-extracted from the adapter module/class bodies
(decorators excluded by lineno, segments dedented); the exec namespace is a
registered sys.modules module (py3.14 annotation resolution).  ``from __future__
import annotations`` keeps signature annotations unevaluated.

Fail-closed: missing defs or 0 cases => exit 2.  Per-case wrapper catches
BaseException (SystemExit included).  Summary:
'ORACLE telegram_adapter_flow9: N cases, M mismatches'.
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

# name -> decorator kind (probe-verified against the live class).
KINDS = {
    "_bold_label_html": "module",
    "_append_observed_note": "staticmethod",
    "_EA_HEADER": "property",
    "_EA_REASON_LABEL": "property",
    "_ea_escape": "instance",
    "_EA_ACTION_LABELS": "property",
    "_EA_SMART_DENY_LINE": "property",
    "_provider_list_text": "staticmethod",
    "_provider_button": "staticmethod",
    "_picker_back_cancel_row": "staticmethod",
    "_picker_nav_row": "staticmethod",
    "_teardown_started": "property",
    "send_path_degraded": "property",
    "_callback_ctx": "staticmethod",
    "supports_draft_streaming": "instance",
    "_fallback_ips": "instance",
    "_link_preview_kwargs": "instance",
    "_build_adapter": "module",
}
# EXACTLY the C ta9_call seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    "html_escape", "t", "InlineKeyboardButton", "TelegramAdapter",
    "resolve_notifications_mode", "LinkPreviewOptions", "parse_fallback_ip_env",
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
    mod = new_module("ta9_oracle_adapter")
    ns = mod.__dict__
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    found = set()
    # module-level targets
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in KINDS:
            exec_node(src, node, ns, A_SRC)
            found.add(node.name)
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "TelegramAdapter")
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in KINDS:
            exec_node(src, node, ns, A_SRC)
            found.add(node.name)
    missing = [k for k in KINDS if k not in ns]
    if missing:
        print(f"FATAL oracle_telegram_adapter_flow9: missing: {missing}")
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


# ── per-case script backend (mirrors the C ta9_call exactly) ──
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


def rec(name, args_fn, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k)
        raw = _CUR.call(name, args)
        return build(raw) if build else raw
    return f


# ── seam stubs (explicit literal names; cross_check scans the rec calls) ──
def build_seams():
    st = {}
    st["html_escape"] = rec("html_escape", lambda value: {"value": value})
    st["t"] = rec("t", lambda key, **kwargs: {"key": key, "kwargs": kwargs})
    st["InlineKeyboardButton"] = rec(
        "InlineKeyboardButton",
        lambda text, callback_data: {"text": text, "callback_data": callback_data})
    st["TelegramAdapter"] = rec(
        "TelegramAdapter", lambda config: {"config": config},
        build=lambda raw: SimpleNamespace(**raw) if isinstance(raw, dict) else raw)
    st["resolve_notifications_mode"] = rec("resolve_notifications_mode", lambda: {})
    st["LinkPreviewOptions"] = rec(
        "LinkPreviewOptions", lambda is_disabled: {"is_disabled": is_disabled})
    st["parse_fallback_ip_env"] = rec(
        "parse_fallback_ip_env", lambda value: {"value": value})
    return st


def ns_double(v):
    if isinstance(v, dict):
        return SimpleNamespace(**{k: ns_double(x) for k, x in v.items()})
    if isinstance(v, list):
        return [ns_double(x) for x in v]
    return v


def mat_state(k, v):
    if k == "_bot" and isinstance(v, dict):
        return SimpleNamespace(**v)
    if k == "config" and isinstance(v, dict):
        return SimpleNamespace(**v)
    return v


def make_doubles(state):
    inst = SimpleNamespace()
    for k, v in (state or {}).items():
        setattr(inst, k, mat_state(k, v))
    return inst


def drive(func, inp):
    global _CUR
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    _CUR = ctx
    st = build_seams()

    # Module globals the extracted defs resolve at call time.
    NS["_html"] = SimpleNamespace(escape=st["html_escape"])
    NS["t"] = st["t"]
    NS["InlineKeyboardButton"] = st["InlineKeyboardButton"]
    NS["TelegramAdapter"] = st["TelegramAdapter"]
    NS["resolve_notifications_mode"] = st["resolve_notifications_mode"]
    NS["parse_fallback_ip_env"] = st["parse_fallback_ip_env"]
    NS["LinkPreviewOptions"] = (st["LinkPreviewOptions"]
                                if state.get("LinkPreviewOptions_present") else None)

    inst = make_doubles(state)

    def A(k):
        return ns_double(args.get(k))

    try:
        if func == "_bold_label_html":
            ret = NS[func](args.get("line"))
        elif func == "_append_observed_note":
            ret = NS[func](args.get("existing"), args.get("note"))
        elif func in ("_EA_HEADER", "_EA_REASON_LABEL", "_EA_ACTION_LABELS",
                      "_EA_SMART_DENY_LINE", "_teardown_started",
                      "send_path_degraded", "_fallback_ips", "_link_preview_kwargs"):
            ret = NS[func](inst)
        elif func == "_ea_escape":
            ret = NS[func](inst, args.get("text"))
        elif func == "_provider_list_text":
            ret = NS[func](args.get("current_model"), args.get("provider_label"),
                           args.get("page_info"))
        elif func == "_provider_button":
            ret = NS[func](args.get("p"))
        elif func == "_picker_back_cancel_row":
            ret = NS[func]()
        elif func == "_picker_nav_row":
            ret = NS[func](args.get("page"), args.get("total_pages"), args.get("prefix"))
        elif func == "_callback_ctx":
            ret = NS[func](A("query"))
        elif func == "supports_draft_streaming":
            ret = NS[func](inst, args.get("chat_type"), args.get("metadata"))
        elif func == "_build_adapter":
            ret = NS[func](A("config"))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(vars(inst))}


# ── cross-checks (C seams / fixtures / C funcs / PoP) ──
def cross_check():
    csrc = (_REPO / "src" / "gateway" / "platforms" /
            "telegram_adapter_flow9.c").read_text(encoding="utf-8")
    c_seams = set(re.findall(r'ta9_call\(\s*ctx,\s*"([^"]+)"', csrc))
    if c_seams != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow9: C seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - c_seams)} extra={sorted(c_seams - set(SEAM_NAMES))}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'rec\(\s*"([^"]+)"', osrc))
    if used != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow9: oracle seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - used)} extra={sorted(used - set(SEAM_NAMES))}")
        sys.exit(2)
    pops = set(re.findall(r'PoP:\s*ta9_\w+\s*@\s*plugins/platforms/telegram/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_telegram_adapter_flow9: PoP/KINDS drift: c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow9.jsonl"
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
            print(f"FATAL oracle_telegram_adapter_flow9: fixture io key set != SEAM_NAMES: "
                  f"missing={sorted(set(SEAM_NAMES) - fix_keys)} extra={sorted(fix_keys - set(SEAM_NAMES))}")
            sys.exit(2)
        if fix_funcs != set(KINDS):
            print(f"FATAL oracle_telegram_adapter_flow9: fixture func set != KINDS: "
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
    print(f"ORACLE telegram_adapter_flow9: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow9.jsonl"
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
    print(f"SELFTEST telegram_adapter_flow9: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
