#!/usr/bin/env python3
"""Oracle for the Feishu adapter Lark-API request-builder / send-result FLOW
port (C sibling: src/gateway/platforms/feishu_adapter_flow5.c).

Drives the LIVE plugins/platforms/feishu/adapter.py builder/result cluster with
SCRIPTED seams: every seamed sibling (_sdk_build, _response_succeeded,
_response_error_result, _extract_response_field) is a recording stub fed the
same scripts the C harness feeds its universal seam fa5_call. Compares the
DECISION SEQUENCE (ordered seam events {"name","args","ret"} in "trace") plus
the final return ("ret") and the (always empty) state.

The live defs are AST-extracted from adapter.py (the 18 class methods; future-
annotations compiler flag; decorators stripped); SendResult is extracted from
gateway/platforms/base.py (decorator kept). The heavy quarry import chain is
never imported.

Mode table (mirrors the C port): each ported function runs its REAL body when
it is the case target; a ported sibling is real only when it is itself the
target and a recorder in every other case.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included); scripted raises round-trip their spec
bare. Summary: 'ORACLE feishu_adapter_flow5: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__":cls,"message":str(exc)}.
- Lark request classes are marker strings; `fields` travels as an ordered
  array of {"name","value"} pairs and projects back to **fields on the live
  side.
- A response is null or a non-empty object with scalar success/data/code/msg
  attributes; `success` is the RESULT of getattr(...success...)() (a bool).
- SendResult projects via dataclasses.asdict to its 8-field dict.
- full builtin exception hierarchy: make_exc() materializes the exact class
  from EXC_CLASSES; BaseException-family kinds are included, and every scripted
  raise propagates verbatim (none of these defs has a try/except).
"""
import ast
import copy
import dataclasses
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = __import__("pathlib").Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "plugins", "platforms", "feishu", "adapter.py")
BASE_SRC = os.path.join(Q, "gateway", "platforms", "base.py")
_FLAGS = _fut.annotations.compiler_flag

TARGETS = [
    "_build_get_message_request", "_build_message_resource_request",
    "_build_get_application_request", "_build_reply_message_body",
    "_build_reply_message_request", "_build_update_message_body",
    "_build_update_message_request", "_build_delete_message_request",
    "_build_create_message_body", "_build_create_message_request",
    "_build_image_upload_body", "_build_image_upload_request",
    "_build_file_upload_request",
    "_response_succeeded", "_extract_response_field",
    "_response_error_result", "_finalize_send_result",
    "_resolve_outbound_file_routing",
]

# marker strings for the Lark request classes referenced by the builders
REQUEST_CLASSES = [
    "GetMessageRequest", "GetMessageResourceRequest", "GetApplicationRequest",
    "ReplyMessageRequestBody", "ReplyMessageRequest", "UpdateMessageRequestBody",
    "UpdateMessageRequest", "DeleteMessageRequest", "CreateMessageRequestBody",
    "CreateMessageRequest", "CreateImageRequestBody", "CreateImageRequest",
    "CreateFileRequest",
]


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def extract():
    mod = types.ModuleType("fa5_oracle_adapter")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    from typing import Any, Dict, List, Literal, Optional, Sequence
    ns.update({
        "SimpleNamespace": types.SimpleNamespace,
        "Any": Any, "Dict": Dict, "List": List, "Literal": Literal,
        "Optional": Optional, "Sequence": Sequence,
        "dataclass": dataclasses.dataclass, "field": dataclasses.field,
        "Path": PurePosixPath,
        # marker module constants (shared with the C port)
        "_FEISHU_OPUS_UPLOAD_EXTENSIONS": {".ogg", ".opus"},
        "_FEISHU_MEDIA_UPLOAD_EXTENSIONS": {".mp4", ".mov", ".avi", ".m4v"},
        "_FEISHU_DOC_UPLOAD_TYPES": {
            ".pdf": "pdf", ".doc": "doc", ".docx": "doc",
            ".xls": "xls", ".xlsx": "xls", ".ppt": "ppt", ".pptx": "ppt",
        },
        "_FEISHU_FILE_UPLOAD_TYPE": "stream",
        "FeishuAdapter": None,
    })
    for rc in REQUEST_CLASSES:
        ns[rc] = rc  # marker string == class name

    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    found = set()
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "FeishuAdapter":
            for ch in node.body:
                if isinstance(ch, (ast.FunctionDef, ast.AsyncFunctionDef)) and ch.name in TARGETS:
                    c = copy.copy(ch)
                    c.decorator_list = []
                    exec_node(src, c, ns, SRC)
                    found.add(ch.name)
        elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in TARGETS:
            exec_node(src, node, ns, SRC)
            found.add(node.name)
    # SendResult dataclass from gateway/platforms/base.py (KEEP the decorator)
    bsrc = open(BASE_SRC, encoding="utf-8").read()
    btree = ast.parse(bsrc)
    for node in btree.body:
        if isinstance(node, ast.ClassDef) and node.name == "SendResult":
            exec_node(bsrc, node, ns, BASE_SRC)
    required = TARGETS + ["SendResult"]
    missing = [t for t in required if t not in ns]
    if missing:
        print(f"FATAL oracle_feishu_adapter_flow5: adapter missing targets: {missing}")
        sys.exit(2)
    return ns


NS = extract()

# ── seam names regex-extracted from the C port ──
_CSRC = open(_REPO / "src" / "gateway" / "platforms" / "feishu_adapter_flow5.c",
             encoding="utf-8").read()
SEAMED = set(re.findall(r'fa5_call\(\s*ctx,\s*"([^"]+)"', _CSRC))
REAL_DEFS = {nm: NS[nm] for nm in SEAMED if nm in NS}


# ── canonical projection ──
class RespDouble:
    """Materialized response object. canon() projects it back to its raw spec."""
    def __init__(self, spec):
        object.__setattr__(self, "_spec", spec)

    def __deepcopy__(self, memo):
        return RespDouble(self._spec)


def canon(v):
    def conv(x):
        if isinstance(x, RespDouble):
            return conv(x._spec)
        if isinstance(x, (frozenset, set)):
            return sorted(conv(i) for i in x)
        if dataclasses.is_dataclass(x) and not isinstance(x, type):
            return conv(dataclasses.asdict(x))
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, BaseException):
            return str(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# ── exception hierarchy (full builtin side) ──
EXC_CLASSES = {
    "Exception": Exception, "BaseException": BaseException,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "OSError": OSError, "IOError": OSError,
    "LookupError": LookupError, "StopIteration": StopIteration, "IndexError": IndexError,
    "ZeroDivisionError": ZeroDivisionError, "AssertionError": AssertionError,
    "NotImplementedError": NotImplementedError, "SyntaxError": SyntaxError,
    "UnicodeError": UnicodeError, "Warning": Warning, "ArithmeticError": ArithmeticError,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "CancelledError": type("CancelledError", (BaseException,), {}),
}

# BaseException family that bypasses `except Exception` (mirrors fa5_is_base_exc).
BASE_EXC = {"BaseException", "SystemExit", "KeyboardInterrupt", "GeneratorExit", "CancelledError"}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = (BaseException,) if kind in BASE_EXC else (Exception,)
        cls = type(kind, cls, {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── seam argument projections ──
def pairs(**fields):
    return [{"name": k, "value": v} for k, v in fields.items()]


ARGMAP = {
    "_sdk_build": lambda cls, **fields: {"cls": cls, "fields": pairs(**fields)},
    "_response_succeeded": lambda response: {"response": response},
    "_response_error_result": lambda response, default_message: {
        "response": response, "default_message": default_message},
    "_extract_response_field": lambda response, field_name: {
        "response": response, "field_name": field_name},
}


# ── per-case script backend (mirrors C fa5_call exactly) ──
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


def make_recorder(ctx, nm):
    argfn = ARGMAP[nm]

    def f(*a, **k):
        return ctx.call(nm, argfn(*a, **k))
    return f


# shared self/class double refreshed per case in install_recorders
SELF = types.SimpleNamespace()


def install_recorders(ctx, target):
    for nm, real in REAL_DEFS.items():
        NS[nm] = real if nm == target else make_recorder(ctx, nm)
    # seams that are NOT ported targets (e.g. _sdk_build) are always recorders
    for nm in SEAMED - set(REAL_DEFS):
        NS[nm] = make_recorder(ctx, nm)
    for nm in ("_response_succeeded", "_response_error_result", "_extract_response_field"):
        setattr(SELF, nm, NS[nm])
    NS["FeishuAdapter"] = SELF


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


# ── response materialization ──
def mat_resp(raw):
    if raw is None:
        return None  # live None -> falsy, no success() call
    if isinstance(raw, dict):
        d = RespDouble(raw)
        if "success" in raw:
            setattr(d, "success", (lambda v=raw["success"]: v))  # callable, like live
        if "data" in raw:
            dv = raw["data"]
            setattr(d, "data", types.SimpleNamespace(**dv) if isinstance(dv, dict) else dv)
        for k in ("code", "msg"):
            if k in raw:
                setattr(d, k, raw[k])
        return d
    return raw


# ── per-case live call ──
def live_call(func, args, ctx):
    g = args.get
    if func == "_build_get_message_request":
        return NS[func](g("message_id", ""))
    if func == "_build_message_resource_request":
        return NS[func](message_id=g("message_id", ""), file_key=g("file_key", ""),
                        resource_type=g("resource_type", ""))
    if func == "_build_get_application_request":
        return NS[func](app_id=g("app_id", ""), lang=g("lang", ""))
    if func == "_build_reply_message_body":
        return NS[func](content=g("content", ""), msg_type=g("msg_type", ""),
                        reply_in_thread=g("reply_in_thread"), uuid_value=g("uuid_value", ""))
    if func == "_build_reply_message_request":
        return NS[func](g("message_id", ""), g("request_body"))
    if func == "_build_update_message_body":
        return NS[func](msg_type=g("msg_type", ""), content=g("content", ""))
    if func == "_build_update_message_request":
        return NS[func](g("message_id", ""), g("request_body"))
    if func == "_build_delete_message_request":
        return NS[func](g("message_id", ""))
    if func == "_build_create_message_body":
        return NS[func](receive_id=g("receive_id", ""), msg_type=g("msg_type", ""),
                        content=g("content", ""), uuid_value=g("uuid_value", ""))
    if func == "_build_create_message_request":
        return NS[func](g("receive_id_type", ""), g("request_body"))
    if func == "_build_image_upload_body":
        return NS[func](image_type=g("image_type", ""), image=g("image"))
    if func == "_build_image_upload_request":
        return NS[func](g("request_body"))
    if func == "_build_file_upload_request":
        return NS[func](g("request_body"))
    if func == "_response_succeeded":
        return NS[func](mat_resp(g("response")))
    if func == "_extract_response_field":
        return NS[func](mat_resp(g("response")), g("field_name", ""))
    if func == "_response_error_result":
        return NS[func](None, mat_resp(g("response")),
                        default_message=g("default_message", ""),
                        override_error=g("override_error"))
    if func == "_finalize_send_result":
        return NS[func](SELF, mat_resp(g("response")), g("default_message", ""))
    if func == "_resolve_outbound_file_routing":
        return NS[func](file_path=g("file_path", ""),
                        requested_message_type=g("requested_message_type", ""))
    raise RuntimeError(f"unknown func {func}")


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    install_recorders(ctx, func)
    try:
        ret = live_call(func, args, ctx)
    except BaseException as exc:
        ret = exc_spec(exc, ctx)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": {}}


# ── cross-checks ──
def cross_check():
    missing_argmap = sorted(SEAMED - set(ARGMAP))
    if missing_argmap:
        print(f"FATAL oracle_feishu_adapter_flow5: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra = sorted(set(ARGMAP) - SEAMED)
    if extra:
        print(f"FATAL oracle_feishu_adapter_flow5: ARGMAP seams absent from C: {extra}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow5.jsonl"
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
            print(f"FATAL oracle_feishu_adapter_flow5: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        missing_funcs = sorted(set(TARGETS) - fix_funcs)
        if missing_funcs:
            print(f"FATAL oracle_feishu_adapter_flow5: targets without fixture cases: {missing_funcs}")
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
    print(f"ORACLE feishu_adapter_flow5: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow5.jsonl"
    n = bad = 0
    counts = {}
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
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            bad += 1
            print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            for nm in TARGETS:
                if f"'{nm}'" in msg:
                    bad += 1
                    print(f"SELFTEST BINDING GAP {func} case {n}: {msg}")
                    break
    print(f"SELFTEST feishu_adapter_flow5: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
