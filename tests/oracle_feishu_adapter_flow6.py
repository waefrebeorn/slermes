#!/usr/bin/env python3
"""Oracle for the Feishu adapter resolution-helpers FLOW port
(C sibling: src/gateway/platforms/feishu_adapter_flow6.py -> .c).

Drives the LIVE plugins/platforms/feishu/adapter.py resolution cluster with
SCRIPTED seams: every collaborator (mimetypes.guess_extension / guess_type,
hashlib.sha256().hexdigest, secrets_match, timestamp_fresh, future.result,
logger.exception/debug and the ported sibling _default_image_media_type /
_normalize) is a recording stub fed the same scripts the C harness feeds its
universal seam fa6_call. Compares the DECISION SEQUENCE (ordered seam events
{"name","args","ret"} in "trace") plus the final return ("ret") and the
(always empty) state.

The live defs are AST-extracted from adapter.py (14 class methods + 2 module
defs; future-annotations compiler flag; decorators stripped); the module
constants SUPPORTED_DOCUMENT_TYPES / _IMAGE_EXTENSIONS / _AUDIO_EXTENSIONS /
_VIDEO_EXTENSIONS are seeded from base.py / adapter.py. The heavy quarry import
chain is never imported.

Mode table (mirrors the C port): each ported function runs its REAL body when
it is the case target; a ported sibling is real only when it is itself the
target and a recorder in every other case. All other collaborators are
recorders in every case.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included); scripted raises round-trip their spec
bare. Summary: 'ORACLE feishu_adapter_flow6: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__":cls,"message":str(exc)}.
- Carriers (config, response, future, normalized) are materialized into real
  attribute doubles; canon() projects them back to their raw JSON spec.
- mimetypes.guess_type results are scripted as 2-element [mime, encoding]
  arrays (the C port indexes [0]); guess_extension results are str/null.
- Fixture domain: ASCII strings (str.strip ASCII whitespace set; re.sub ASCII
  \\w set; POSIX '/' paths), scalar header/constant values, bytes body passed
  as a str. Documented exclusions are enforced by the fixture generator.
"""
import ast
import copy
import json
import os
import re
import sys
import textwrap
import types
import posixpath
import re as _re_module
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
    "_is_connected",
    "_normalize_media_type", "_default_image_media_type", "_map_chat_type",
    "_resolve_source_chat_type", "_guess_remote_extension",
    "_display_name_from_cached_path", "_get_response_header",
    "_extract_text_from_raw_content", "_guess_extension",
    "_guess_document_media_type", "_guess_media_type_from_filename",
    "_derive_remote_filename", "_log_background_failure",
    "_is_webhook_signature_valid", "_namespace_from_mapping",
]

# Seams that are ported targets (real only when they are the case target).
PORTED_SIBLINGS = {"_default_image_media_type", "_normalize",
                   "_namespace_from_mapping"}


def _lit_constant(path, name):
    """Read a module-level literal constant from a source file via AST."""
    tree = ast.parse(open(path, encoding="utf-8").read())
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            for t in tgts:
                if isinstance(t, ast.Name) and t.id == name:
                    return ast.literal_eval(node.value)
    raise KeyError(name)


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def extract():
    mod = types.ModuleType("fa6_oracle_adapter")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    from typing import Any, Dict, List, Literal, Optional, Sequence
    ns.update({
        "Path": PurePosixPath,
        "os": types.SimpleNamespace(path=posixpath),
        "re": _re_module,
        "SimpleNamespace": types.SimpleNamespace,
        "Any": Any, "Dict": Dict, "List": List, "Literal": Literal,
        "Optional": Optional, "Sequence": Sequence,
        # module constants (shared with the C port)
        "SUPPORTED_DOCUMENT_TYPES": _lit_constant(BASE_SRC, "SUPPORTED_DOCUMENT_TYPES"),
        "_IMAGE_EXTENSIONS": _lit_constant(SRC, "_IMAGE_EXTENSIONS"),
        "_AUDIO_EXTENSIONS": _lit_constant(SRC, "_AUDIO_EXTENSIONS"),
        "_VIDEO_EXTENSIONS": _lit_constant(SRC, "_VIDEO_EXTENSIONS"),
        "FeishuAdapter": None,
    })

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
    missing = [t for t in TARGETS if t not in ns]
    if missing:
        print(f"FATAL oracle_feishu_adapter_flow6: adapter missing targets: {missing}")
        sys.exit(2)
    return ns


NS = extract()

# ── seam names regex-extracted from the C port ──
_CSRC = open(_REPO / "src" / "gateway" / "platforms" / "feishu_adapter_flow6.c",
             encoding="utf-8").read()
SEAMED = set(re.findall(r'fa6_call\(\s*ctx,\s*"([^"]+)"', _CSRC))
REAL_DEFS = {nm: NS[nm] for nm in SEAMED if nm in NS}


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
def _sha_arg(data):
    if isinstance(data, (bytes, bytearray)):
        return {"text": bytes(data).decode("utf-8", errors="replace")}
    return {"text": str(data)}


ARGMAP = {
    "_default_image_media_type": lambda ext: {"ext": ext},
    "_normalize": lambda message_type, raw_content, mentions: {
        "message_type": message_type, "raw_content": raw_content, "mentions": mentions},
    "future_result": lambda: {},
    "logger_exception": lambda message, *a, **k: {"message": message},
    "logger_debug": lambda message, *a, **k: {"message": message},
    "sha256_hex": _sha_arg,
    "secrets_match": lambda signature, computed: {"signature": signature, "computed": computed},
    "timestamp_fresh": lambda timestamp: {"timestamp": timestamp},
    "mimetypes_guess_extension": lambda content_type: {"content_type": content_type},
    "mimetypes_guess_type": lambda filename: {"filename": filename},
}

# Post-processing of a recorder's RESULT before it reaches the live body
# (the trace already carries the raw, canonical result).
def _wrap_guess_type(r):
    if isinstance(r, (list, tuple)):
        return tuple(r)
    if isinstance(r, str):
        return (r, None)
    return (None, None)


RESULT_WRAP = {
    "_normalize": lambda r: types.SimpleNamespace(**r) if isinstance(r, dict) else r,
    "mimetypes_guess_type": _wrap_guess_type,
}


# ── per-case script backend (mirrors C fa6_call exactly) ──
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
    wrap = RESULT_WRAP.get(nm)

    def f(*a, **k):
        r = ctx.call(nm, argfn(*a, **k))
        return wrap(r) if wrap else r
    return f


class _HexDigest:
    def __init__(self, data, ctx):
        self._data = data
        self._ctx = ctx

    def hexdigest(self):
        return self._ctx.call("sha256_hex", _sha_arg(self._data))


def _make_sha256(ctx):
    def sha256(data):
        return _HexDigest(data, ctx)
    return sha256


# shared self double refreshed per case in install_recorders
SELF = types.SimpleNamespace()


def install_recorders(ctx, target):
    for nm, real in REAL_DEFS.items():
        NS[nm] = real if nm == target else make_recorder(ctx, nm)
    for nm in SEAMED - set(REAL_DEFS):
        NS[nm] = make_recorder(ctx, nm)
    NS["mimetypes"] = types.SimpleNamespace(
        guess_extension=make_recorder(ctx, "mimetypes_guess_extension"),
        guess_type=make_recorder(ctx, "mimetypes_guess_type"))
    NS["hashlib"] = types.SimpleNamespace(sha256=_make_sha256(ctx))
    NS["secrets_match"] = make_recorder(ctx, "secrets_match")
    NS["timestamp_fresh"] = make_recorder(ctx, "timestamp_fresh")
    NS["logger"] = types.SimpleNamespace(
        debug=make_recorder(ctx, "logger_debug"),
        exception=make_recorder(ctx, "logger_exception"),
        warning=lambda *a, **k: None,
        info=lambda *a, **k: None)
    # Self double: ported siblings used through FeishuAdapter.<name>.
    SELF.__dict__.clear()
    for nm in PORTED_SIBLINGS:
        if nm in NS:
            setattr(SELF, nm, NS[nm])
    NS["FeishuAdapter"] = SELF


def exc_spec(exc, ctx):
    for e, spec in ctx.scripted:
        if exc is e:
            return spec
    return proj_exc(exc)


# ── carrier materialization ──
def mat_simple(raw):
    return types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw


def mat_response(raw):
    if raw is None:
        return None
    d = types.SimpleNamespace()
    r = raw.get("raw") if isinstance(raw, dict) else None
    setattr(d, "raw", types.SimpleNamespace(**r) if isinstance(r, dict) else r)
    return d


def mat_future():
    return types.SimpleNamespace(result=make_recorder(_ACTIVE_CTX[0], "future_result"))


_ACTIVE_CTX = [None]


# ── per-case live call ──
def live_call(func, args, ctx):
    _ACTIVE_CTX[0] = ctx
    g = args.get
    if func == "_is_connected":
        return NS[func](mat_simple(g("config")))
    if func == "_normalize_media_type":
        return NS[func](g("content_type", ""), default=g("default", ""))
    if func == "_default_image_media_type":
        return NS[func](g("ext", ""))
    if func == "_map_chat_type":
        return NS[func](g("raw_chat_type", ""))
    if func == "_resolve_source_chat_type":
        return NS[func](chat_info=(g("chat_info") or {}), event_chat_type=g("event_chat_type", ""))
    if func == "_guess_remote_extension":
        return NS[func](g("url", ""), default=g("default", ""))
    if func == "_display_name_from_cached_path":
        return NS[func](g("path", ""))
    if func == "_get_response_header":
        return NS[func](mat_response(g("response")), g("name", ""))
    if func == "_extract_text_from_raw_content":
        return NS[func](SELF, msg_type=g("msg_type", ""), raw_content=g("raw_content", ""),
                        mentions=g("mentions"))
    if func == "_guess_extension":
        return NS[func](g("filename", ""), g("content_type", ""), g("default", ""),
                        allowed=set(g("allowed") or []))
    if func == "_guess_document_media_type":
        return NS[func](g("filename", ""))
    if func == "_guess_media_type_from_filename":
        return NS[func](g("filename", ""))
    if func == "_derive_remote_filename":
        return NS[func](g("file_url", ""), content_type=g("content_type", ""),
                        default_name=g("default_name", ""), default_ext=g("default_ext", ""))
    if func == "_log_background_failure":
        return NS[func](mat_future())
    if func == "_is_webhook_signature_valid":
        setattr(SELF, "_encrypt_key", g("encrypt_key"))
        body = g("body_bytes", "")
        body_bytes = body.encode("utf-8", errors="replace") if isinstance(body, str) else body
        return NS[func](SELF, (g("headers") or {}), body_bytes)
    if func == "_namespace_from_mapping":
        return NS[func](g("value"))
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
        print(f"FATAL oracle_feishu_adapter_flow6: C seams without ARGMAP: {missing_argmap}")
        sys.exit(2)
    extra = sorted(set(ARGMAP) - SEAMED)
    if extra:
        print(f"FATAL oracle_feishu_adapter_flow6: ARGMAP seams absent from C: {extra}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow6.jsonl"
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
            print(f"FATAL oracle_feishu_adapter_flow6: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        missing_funcs = sorted(set(TARGETS) - fix_funcs)
        if missing_funcs:
            print(f"FATAL oracle_feishu_adapter_flow6: targets without fixture cases: {missing_funcs}")
            sys.exit(2)
        unknown_funcs = sorted(fix_funcs - set(TARGETS))
        if unknown_funcs:
            print(f"FATAL oracle_feishu_adapter_flow6: fixture funcs not in TARGETS: {unknown_funcs}")
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
    print(f"ORACLE feishu_adapter_flow6: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "feishu_adapter_flow6.jsonl"
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
        if case.get("_note") == "fail-closed":
            continue
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
    print(f"SELFTEST feishu_adapter_flow6: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
