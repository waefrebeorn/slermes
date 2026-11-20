#!/usr/bin/env python3
"""Shadow mirror of src/agent/transports/transport_types_flow2.c — a literal Python
transcription of the C algorithms (tt2_project / tt2_init / tt2_repr_value /
tt2_repr_string / tt2_repr_double / tt2_deep_eq / tt2_eq / tt2_replace + the fail-closed
universal-seam dispatcher). It does NOT call Python repr()/== for the ported logic; it
re-implements the C byte-for-byte so that diffing the mirror against the LIVE drive
proves the C transcription is faithful (a mirror that just called repr/== would prove
nothing). Used code-only (no compile/link): the mirror emits the same {func,in,out}
envelopes the C harness emits, which are piped into the oracle's REAL main().
"""
import math

# field tables: (name, default, repr_flag) — declaration order, mirrors the C arrays.
TOOL_CALL = [("id", None, True), ("name", None, True), ("arguments", None, True),
             ("provider_data", None, False)]
USAGE = [("prompt_tokens", 0, True), ("completion_tokens", 0, True),
         ("total_tokens", 0, True), ("cached_tokens", 0, True)]
NORMALIZED_RESPONSE = [("content", None, True), ("tool_calls", None, True),
                       ("finish_reason", None, True), ("reasoning", None, True),
                       ("usage", None, True), ("provider_data", None, False)]
FT = {"ToolCall": TOOL_CALL, "Usage": USAGE, "NormalizedResponse": NORMALIZED_RESPONSE}
CLS_OF_FUNC = {
    "tool_call": "ToolCall", "usage": "Usage", "normalized_response": "NormalizedResponse",
}


def _copy(v):
    # deep copy via JSON round-trip is overkill; values are plain JSON, copy structurally.
    if isinstance(v, dict):
        return {k: _copy(x) for k, x in v.items()}
    if isinstance(v, list):
        return [_copy(x) for x in v]
    return v


def project(fields, ft):
    fields = fields or {}
    o = {}
    for name, default, _repr in ft:
        if name in fields:                 # PRESENT (incl. null) verbatim
            o[name] = _copy(fields[name])
        else:                              # ABSENT -> default
            o[name] = default
    return o


def init(fields, ft, cls):
    fields = fields or {}
    names = {f[0] for f in ft}
    for k in fields:                       # first unexpected, in passed order
        if k not in names:
            return {"__raise__": "TypeError",
                    "msg": "%s.__init__() got an unexpected keyword argument '%s'" % (cls, k)}
    return project(fields, ft)


# ── tt2_repr_string transcription ──
def repr_string(s):
    if s is None:
        s = ""
    quote = "'"
    if "'" in s and '"' not in s:
        quote = '"'
    out = [quote]
    for ch in s:
        c = ord(ch)
        if ch == "\\":
            out.append("\\\\")
        elif ch == quote:
            out.append("\\" + ch)
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\r":
            out.append("\\r")
        elif ch == "\t":
            out.append("\\t")
        elif c < 0x20 or c == 0x7f:
            out.append("\\x%02x" % c)
        else:
            out.append(ch)
    out.append(quote)
    return "".join(out)


# ── tt2_repr_double transcription ──
def repr_double(v):
    d = float(v)
    if math.isfinite(d) and d == math.floor(d) and abs(d) < 1e16:
        return "%.0f" % d
    if not math.isfinite(d):
        if math.isnan(d):
            return "nan"
        return "inf" if d > 0 else "-inf"
    s = None
    for prec in range(1, 18):
        s = "%.*g" % (prec, d)
        if float(s) == d:
            break
    return s


# ── tt2_repr_value transcription ──
def repr_value(v):
    if v is None:
        return "None"
    if isinstance(v, bool):
        return "True" if v else "False"
    if isinstance(v, (int, float)):
        return repr_double(v)
    if isinstance(v, str):
        return repr_string(v)
    if isinstance(v, list):
        return "[" + ", ".join(repr_value(x) for x in v) + "]"
    if isinstance(v, dict):
        return "{" + ", ".join(repr_string(k) + ": " + repr_value(x) for k, x in v.items()) + "}"
    return str(v)


def repr_(fields, ft, cls):
    proj = project(fields, ft)
    parts = []
    for name, _default, repr_flag in ft:
        if not repr_flag:
            continue
        parts.append("%s=%s" % (name, repr_value(proj[name])))
    return "%s(%s)" % (cls, ", ".join(parts))


# ── tt2_deep_eq transcription (Python == on JSON values) ──
def _numeric(v):
    if isinstance(v, bool):
        return 1.0 if v else 0.0
    if isinstance(v, (int, float)):
        return float(v)
    return None


def deep_eq(a, b):
    if a is None or b is None:
        return a is None and b is None
    na, nb = _numeric(a), _numeric(b)
    if na is not None and nb is not None:
        return na == nb
    if isinstance(a, str) and isinstance(b, str):
        return a == b
    if isinstance(a, list) and isinstance(b, list):
        return len(a) == len(b) and all(deep_eq(x, y) for x, y in zip(a, b))
    if isinstance(a, dict) and isinstance(b, dict):
        if len(a) != len(b):
            return False
        for k in a:
            if k not in b:
                return False
            if not deep_eq(a[k], b[k]):
                return False
        return True
    return False


def eq(self_fields, other_fields, other_class, ft, cls):
    if other_class and other_class != cls:
        return False
    a = project(self_fields, ft)
    b = project(other_fields, ft)
    return all(deep_eq(a[name], b[name]) for name, _d, _r in ft)


# ── fail-closed universal-seam dispatcher (mirrors harness script_call + tt2_call) ──
class SeamCtx:
    def __init__(self, scripts):
        self.scripts = scripts or {}
        self.idx = {}
        self.trace = []

    def call(self, name, args):
        if name not in self.scripts:
            r = {"__raise__": "RuntimeError", "msg": "no script for seam: %s" % name}
        else:
            sc = self.scripts[name]
            if isinstance(sc, list):
                i = self.idx.get(name, 0)
                self.idx[name] = i + 1
                r = _copy(sc[min(i, len(sc) - 1)]) if sc else None
            else:
                r = _copy(sc)
        # NULL == None: a scripted null is None (never a sentinel node).
        ev_args = {k: _copy(v) for k, v in args.items() if v is not None}
        self.trace.append({"ev": name, "args": ev_args, "ret": _copy(r)})
        return r


def _is_raise(r):
    return isinstance(r, dict) and "__raise__" in r


def replace(ctx, self_fields, changes, ft, cls, seam_name):
    merged = {}
    for k, v in (changes or {}).items():
        merged[k] = _copy(v)
    self_proj = project(self_fields, ft)
    for name, _d, _r in ft:
        if name not in merged:
            merged[name] = _copy(self_proj[name])
    r = ctx.call(seam_name, merged)
    if _is_raise(r):
        return r                          # no except site: every kind passes through
    return init(merged, ft, cls)


def run_case(func, inp):
    """Return (ret, trace) exactly as the C harness would emit."""
    prefix, meth = func.rsplit("_", 1)
    cls = CLS_OF_FUNC[prefix]
    ft = FT[cls]
    ctx = SeamCtx(inp.get("seams", {}))
    if meth == "init":
        ret = init(inp.get("fields"), ft, cls)
    elif meth == "repr":
        ret = repr_(inp.get("fields"), ft, cls)
    elif meth == "eq":
        ret = eq(inp.get("self"), inp.get("other"), inp.get("other_class"), ft, cls)
    elif meth == "replace":
        ret = replace(ctx, inp.get("self"), inp.get("changes"), ft, cls,
                      "%s.__init__" % cls)
    else:
        raise ValueError("unknown func: %s" % func)
    return ret, ctx.trace


def envelope(func, inp):
    ret, trace = run_case(func, inp)
    return {"func": func, "in": inp, "out": {"ret": ret, "trace": trace}}