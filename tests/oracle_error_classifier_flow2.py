#!/usr/bin/env python3
"""Oracle for the FLOW2 port of the NINE unclaimed leaves of
agent/error_classifier.py (src/agent/error_classifier_flow2.c, static ec2_*).

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"] with AST-extracted defs from the quarry, and
compares the ordered trace, the return, and the state projection ({}.

The compiled regex OBJECTS are not ported to C: they are recordable
collaborators. The generator computes each pattern's real answer and scripts it
into in.io; the live drive below replaces those module globals with recorder
objects that read the SAME scripts, so both sides agree by construction on the
regex answers and the C control flow is what is actually tested. Claimed
siblings (_first_match, _has_usage_limit_transient_signal, _classify_402) and
the verdict constants are scripted the same way.

Full builtin exception hierarchy is honored by build_exc()/exc_class(); a
scripted __raise__ propagates out of every one of these nine defs (none has an
`except` site). Summary: 'ORACLE error_classifier_flow2: N cases, M mismatches'.
"""
import ast
import builtins
import enum
import json
import math
import os
import re
import sys
import types
import textwrap
import __future__
from pathlib import Path
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "error_classifier.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))

EC_SRC = str(QUARRY / "agent" / "error_classifier.py")
ANN_FLAG = __future__.annotations.compiler_flag
NAME = "error_classifier_flow2"

FUNCS = [
    "_is_byte_size_limit_message",
    "is_reasoning_level_rejection",
    "is_reasoning_required_rejection",
    "is_reasoning_field_rejection",
    "_account_gates_model",
    "_by_message",
    "_has_large_inline_image",
    "_oversized_message_content_rejection",
    "_classify_image_tool_422",
]

# live global -> seam name. Regex objects get wrapped (RegexSeam); the plain
# callables get recorder stubs. Verdict constants are set per-case from io.
REGEX_SEAMS = {
    "_BYTE_SIZE_OVERFLOW": "byte_size_overflow",
    "_TOKEN_COUNT": "token_count",
    "_BYTE_SIZE_SUBJECT": "byte_size_subject",
    "_REASONING_FIELD_TOKEN": "reasoning_field_token",
    "_REASONING_PARAM_REJECTION": "reasoning_param_rejection",
    "_ACCOUNT_GATING_TAIL_RE": "account_gating_tail",
    "_ACCOUNT_GATED_MODEL_RE": "account_gated_model",
}
CLAIMED_SEAMS = {
    "_first_match": "first_match",
    "_has_usage_limit_transient_signal": "has_usage_limit_transient_signal",
    "_classify_402": "classify_402",
}
RULES_LABELS = {
    "_MESSAGE_HEAD_RULES": "head",
    "_MESSAGE_TAIL_RULES": "tail",
    "_IMAGE_TOOL_RULES": "image_tool",
}
VERDICT_CONSTS = [
    "V_PAYLOAD_TOO_LARGE", "V_RATE_LIMIT", "V_BILLING",
    "V_MODEL_ENTITLEMENT", "V_IMAGE_TOO_LARGE", "V_FORMAT_ERROR",
]

CURRENT = {"rec": None}
_MISSING = object()


def exc_class(kind):
    b = getattr(builtins, kind, None)
    if isinstance(b, type) and issubclass(b, BaseException):
        return b
    return type(kind, (Exception,), {})


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


def build_exc(spec):
    return exc_class(spec.get("__raise__", "RuntimeError"))(spec.get("message", ""))


class Script:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.handed = []

    def next(self, name):
        entry = self.io.get(name, _MISSING)
        if entry is _MISSING:
            return self._fail(name)
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return self._fail(name)
            self.cursors[name] = idx + 1
            val = entry[idx]
        else:
            val = entry
        return self._unwrap(name, val)

    def _fail(self, name):
        spec = {"__raise__": "RuntimeError", "message": "no script for " + name}
        r = Raise(spec, build_exc(spec))
        self.handed.append(r)
        return r

    def _unwrap(self, name, val):
        if isinstance(val, dict) and "__raise__" in val:
            r = Raise(val, build_exc(val))
            self.handed.append(r)
            return r
        if isinstance(val, dict) and val.get("__script__") is True:
            return val.get("ret")
        return val


def norm_value(v):
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, enum.Enum):
        return v.value
    if isinstance(v, dict):
        return {k: norm_value(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_value(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted(norm_value(x) for x in v)
    if isinstance(v, float):
        if math.isfinite(v) and v == int(v):
            return int(v)
        return v
    return v


def norm_args(v):
    v = norm_value(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_args(x) for x in v]
    return v


# ---- seam projection (mirrors the C {..} argument builders byte-for-byte) ----
def proj(name, a, k):
    if name in ("byte_size_overflow", "token_count", "byte_size_subject",
                "reasoning_field_token", "reasoning_param_rejection",
                "account_gating_tail", "wall_words", "long_quota_window",
                "usage_limit", "request_words"):
        return {"msg": a[0]}
    if name == "account_gated_model":
        return {"msg": a[0], "start": a[1]}
    if name == "first_match":
        return {"msg": a[0], "rules": a[1]}
    if name == "has_usage_limit_transient_signal":
        return {"error_msg": a[0], "body": a[1], "response_headers": a[2]}
    if name == "classify_402":
        return {"error_msg": a[0], "code": a[2], "body": a[3]}
    return {**{f"arg{i}": v for i, v in enumerate(a)}, **k}


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.script = Script(inp.get("io"))

    def recorder(self, name):
        ctx = self

        def call(*a, **k):
            args = norm_args(proj(name, a, k))
            raw = ctx.script.next(name)
            if isinstance(raw, Raise):
                ctx.trace.append({"ev": name, "args": args, "ret": raw.spec})
                raise raw.exc
            ctx.trace.append({"ev": name, "args": args, "ret": norm_value(raw)})
            return raw
        return call


class _Match:
    """Minimal re.Match surface used by the live defs."""
    def __init__(self, s, e):
        self._s, self._e = s, e

    def start(self):
        return self._s

    def end(self):
        return self._e


class SeamPatterns:
    """Replaces a `for p in <pattern-tuple>` module global so live's `any()`
    fires the corresponding seam once, and yields a truth-equivalent probe."""
    def __init__(self, seam):
        self.seam = seam

    def __iter__(self):
        msg = CURRENT.get("msg", "")
        raw = CURRENT["rec"](self.seam)(msg)
        return iter([msg] if raw else ())


class RegexSeam:
    """Replaces a compiled-pattern module global; .search/.finditer fire seams."""
    def __init__(self, name):
        self.name = name

    def _call(self, s):
        return CURRENT["rec"](self.name)(s)

    def search(self, s, pos=0, endpos=None):
        if endpos is not None:
            raw = CURRENT["rec"](self.name)(s, endpos)
        else:
            raw = CURRENT["rec"](self.name)(s)
        if isinstance(raw, dict) and "start" in raw:
            return _Match(int(raw["start"]), int(raw.get("end", raw["start"] + 1)))
        if raw:
            return _Match(pos, pos + 1)
        return None

    def finditer(self, s, pos=0, endpos=None):
        raw = self._call(s)
        out = []
        for d in (raw or []):
            st = int(d["start"])
            out.append(_Match(st, int(d.get("end", st + 1))))
        return iter(out)


class ReProxy:
    """Only re.split(r'[^a-z0-9_]+', msg) is used at call time; seam it."""
    def __init__(self, real):
        self._real = real

    def split(self, pattern, string, *a, **k):
        if pattern == r"[^a-z0-9_]+":
            return CURRENT["rec"]("request_words")(string)
        return self._real.split(pattern, string, *a, **k)

    def __getattr__(self, name):
        return getattr(self._real, name)


class LabeledRules(tuple):
    """A rules tuple carrying the C-side seam label."""
    def __new__(cls, label, items):
        o = super().__new__(cls, items)
        o.label = label
        return o


# ---- live namespace seeding ----
NS = {}
REAL = {}


def _exec_seg(src, node, ns, filename):
    seg = textwrap.dedent(ast.get_source_segment(src, node))
    code = compile(seg, filename, "exec", flags=ANN_FLAG, dont_inherit=True)
    exec(code, ns)


def load_ns():
    global NS, REAL
    mod = types.ModuleType("ec2_live")
    sys.modules["ec2_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "ec2_live", "enum": enum, "math": math, "json": json,
        "re": ReProxy(re), "itertools": __import__("itertools"),
        "Any": object, "Optional": object, "Callable": object, "Dict": dict,
        "Iterator": object, "Sequence": object,
        "dataclass": lambda c: c, "field": lambda **k: None,
    })
    src = Path(EC_SRC).read_text(encoding="utf-8")
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "FailoverReason":
            _exec_seg(src, node, ns, EC_SRC)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in FUNCS:
            _exec_seg(src, node, ns, EC_SRC)
    # module constants (plain Name / tuple-of-Name targets)
    skip = {"logger", "_STATUS_HANDLERS"}
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            if isinstance(node, ast.AnnAssign) and node.value is None:
                continue
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]

            def flat(t):
                if isinstance(t, ast.Name):
                    return [t.id]
                if isinstance(t, (ast.Tuple, ast.List)):
                    out = []
                    for e in t.elts:
                        out += flat(e)
                    return out
                return None
            names = []
            for t in tgts:
                got = flat(t)
                if got is None:
                    names = None
                    break
                names += got
            if not names or any(n in skip or n.startswith("logger") for n in names):
                continue
            try:
                _exec_seg(src, node, ns, EC_SRC)
            except Exception:
                pass
    missing = [n for n in FUNCS if n not in ns]
    if missing:
        raise RuntimeError("missing target defs: %s" % missing)

    # regex objects -> recorders
    for live, seam in REGEX_SEAMS.items():
        ns[live] = RegexSeam(seam)
    # `any(p in msg for p in <tuple>)` membership tables -> recorders
    for live, seam in (("_LONG_QUOTA_WINDOW_PATTERNS", "long_quota_window"),
                       ("_USAGE_LIMIT_PATTERNS", "usage_limit"),
                       ("_WALL_WORDS", "wall_words")):
        ns[live] = SeamPatterns(seam)
    # claimed siblings -> recorders. _first_match reads rules.label.
    def _first_match_stub(msg, rules):
        return CURRENT["rec"]("first_match")(msg, getattr(rules, "label", ""))
    ns["_first_match"] = _first_match_stub
    ns["_has_usage_limit_transient_signal"] = _stub("has_usage_limit_transient_signal")
    def _classify_402_stub(msg, result_fn, code="", body=None):
        return CURRENT["rec"]("classify_402")(msg, result_fn, code, body)
    ns["_classify_402"] = _classify_402_stub

    # in-function import of the shrink target: stub parent + leaf module
    agent_pkg = types.ModuleType("agent")
    agent_pkg.__path__ = []
    sys.modules["agent"] = agent_pkg
    shrink_mod = types.ModuleType("agent.conversation_compression_image_shrink")
    sys.modules["agent.conversation_compression_image_shrink"] = shrink_mod
    agent_pkg.conversation_compression_image_shrink = shrink_mod

    REAL = {nm: ns[nm] for nm in FUNCS}


def _stub(seam):
    def f(*a, **k):
        return CURRENT["rec"](seam)(*a, **k)
    return f


def setup_case(inp):
    """Per-case: verdict constants, labeled rules, shrink constant."""
    io = inp.get("io") or {}
    for v in VERDICT_CONSTS:
        if v in io:
            NS["_" + v] = io[v]
    for rname, label in RULES_LABELS.items():
        NS[rname] = LabeledRules(label, NS.get(rname) or ())
    shrink_mod = sys.modules.get("agent.conversation_compression_image_shrink")
    if shrink_mod is not None:
        shrink_mod._IMAGE_SHRINK_TARGET_BYTES = io.get(
            "image_shrink_target_bytes", 4 * 1024 * 1024)


def drive(func, inp):
    msg = inp.get("msg") or ""
    CURRENT["msg"] = msg   # SeamPatterns membership probe
    if func == "_is_byte_size_limit_message":
        return NS[func](inp.get("msg")), {}
    if func in ("is_reasoning_level_rejection", "is_reasoning_required_rejection",
                "is_reasoning_field_rejection", "_account_gates_model"):
        return NS[func](msg), {}
    if func == "_by_message":
        c = SimpleNamespace(msg=msg, body=inp.get("body"),
                            headers=inp.get("response_headers"))
        return NS[func](c), {}
    if func == "_has_large_inline_image":
        return NS[func](inp.get("content")), {}
    if func == "_oversized_message_content_rejection":
        return NS[func](inp.get("body")), {}
    if func == "_classify_image_tool_422":
        c = SimpleNamespace(msg=msg, body=inp.get("body"))
        return NS[func](c), {}
    raise RuntimeError("unknown func " + func)


def run_case(func, inp):
    # restore real ported defs each case (composed siblings stay real)
    for nm, fn in REAL.items():
        NS[nm] = fn
    setup_case(inp)
    ctx = CaseCtx(inp)
    CURRENT["rec"] = ctx.recorder
    try:
        ret, st = drive(func, inp)
        return {"ret": norm_value(ret), "trace": ctx.trace, "st": norm_value(st)}
    except BaseException as e:
        spec = None
        for r in ctx.script.handed:
            if e is r.exc:
                spec = r.spec
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        return {"ret": spec, "trace": ctx.trace, "st": {}}


def selftest():
    load_ns()
    fx = _REPO / "tests" / "fixtures" / (NAME + ".jsonl")
    n = 0
    for line in fx.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        rec = json.loads(line)
        r = run_case(rec["func"], rec.get("in") or {})
        n += 1
        rr = r.get("ret")
        if isinstance(rr, dict) and "__raise__" in rr:
            print("RAISE %-40s %s" % (rec["func"], json.dumps(rr)))
        if r["trace"] is None:
            print("BAD TRACE", rec["func"])
    print("selftest: %d cases driven" % n)


def main():
    load_ns()
    n = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        func = rec.get("func")
        inp = rec.get("in") or {}
        n += 1
        want = run_case(func, inp)
        got = norm_value({"ret": rec.get("ret"), "trace": rec.get("trace"),
                          "st": rec.get("st")})
        if got != want:
            mism += 1
            if mism <= 15:
                print("MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:900]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:900]))
    print("ORACLE %s: %d cases, %d mismatches" % (NAME, n, mism))
    if n == 0:
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        selftest()
    else:
        main()
