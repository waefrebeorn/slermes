#!/usr/bin/env python3
"""Shadow mirror of src/agent/account_usage_nous_flow2.c (aun2_*).

A literal Python transcription of the C port's JSON-level decision path +
universal-seam dispatcher (script queue cursors, fail-closed raise specs,
strip-nulls args, ordered trace). Operates on RAW JSON values (dict/list/str/
int/float/bool/None) exactly as the C harness does -- NO live quarry code. Used
to verify the port logic against the live oracle drive over every fixture
without compiling/linking (code-only verification). Emits {func,in,ret,trace,st}
envelopes identical to the C harness output.
"""
import json
import math
import struct

MISSING = object()

# ---- exception hierarchy (transcribed from AUN2_EXC_PARENT) ----
EXC_PARENT = {
    "BaseException": "object", "BaseExceptionGroup": "BaseException", "Exception": "BaseException",
    "GeneratorExit": "BaseException", "KeyboardInterrupt": "BaseException", "SystemExit": "BaseException",
    "CancelledError": "BaseException", "ArithmeticError": "Exception", "AssertionError": "Exception",
    "AttributeError": "Exception", "BufferError": "Exception", "EOFError": "Exception",
    "EnvironmentError": "OSError", "IOError": "OSError", "ImportError": "Exception",
    "LookupError": "Exception", "MemoryError": "Exception", "NameError": "Exception",
    "OSError": "Exception", "ReferenceError": "Exception", "RuntimeError": "Exception",
    "StopAsyncIteration": "Exception", "StopIteration": "Exception", "SyntaxError": "Exception",
    "SystemError": "Exception", "TypeError": "Exception", "ValueError": "Exception",
    "Warning": "Exception", "WindowsError": "OSError", "FloatingPointError": "ArithmeticError",
    "OverflowError": "ArithmeticError", "ZeroDivisionError": "ArithmeticError",
    "ModuleNotFoundError": "ImportError", "IndexError": "LookupError", "KeyError": "LookupError",
    "UnboundLocalError": "NameError", "BlockingIOError": "OSError", "ChildProcessError": "OSError",
    "ConnectionError": "OSError", "FileExistsError": "OSError", "FileNotFoundError": "OSError",
    "InterruptedError": "OSError", "IsADirectoryError": "OSError", "NotADirectoryError": "OSError",
    "PermissionError": "OSError", "ProcessLookupError": "OSError", "TimeoutError": "OSError",
    "BrokenPipeError": "ConnectionError", "ConnectionAbortedError": "ConnectionError",
    "ConnectionRefusedError": "ConnectionError", "ConnectionResetError": "ConnectionError",
    "NotImplementedError": "RuntimeError", "PythonFinalizationError": "RuntimeError",
    "RecursionError": "RuntimeError", "IndentationError": "SyntaxError",
    "_IncompleteInputError": "SyntaxError", "TabError": "IndentationError",
    "UnicodeError": "ValueError", "UnicodeDecodeError": "UnicodeError",
    "UnicodeEncodeError": "UnicodeError", "UnicodeTranslateError": "UnicodeError",
    "BytesWarning": "Warning", "DeprecationWarning": "Warning", "EncodingWarning": "Warning",
    "FutureWarning": "Warning", "ImportWarning": "Warning", "PendingDeprecationWarning": "Warning",
    "ResourceWarning": "Warning", "RuntimeWarning": "Warning", "SyntaxWarning": "Warning",
    "UnicodeWarning": "Warning", "UserWarning": "Warning", "FrozenInstanceError": "AttributeError",
}


def is_exception(kind):
    if kind == "ExceptionGroup":
        return True
    cur = kind
    for _ in range(32):
        if cur is None:
            return True
        if cur == "Exception":
            return True
        if cur in ("BaseException", "object"):
            return False
        cur = EXC_PARENT.get(cur)
        if cur is None:
            return True
    return True


def caught_by(kind, names):
    cur = kind
    for _ in range(32):
        if cur is None:
            return False
        if cur in names:
            return True
        if cur in ("BaseException", "object"):
            return False
        cur = EXC_PARENT.get(cur)
        if cur is None:
            return False
    return False


ATE = ("AttributeError", "TypeError")


def is_raise(r):
    return isinstance(r, dict) and "__raise__" in r


def kind(r):
    return r.get("__raise__", "")


def mkraise(k, m):
    return {"__raise__": k, "message": m}


# ---- truthiness / accessors (JSON level) ----
def truth(v):
    if v is None:
        return False
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return v != 0
    if isinstance(v, str):
        return len(v) > 0
    if isinstance(v, (list, dict)):
        return len(v) > 0
    return False


def present(v):
    return v is not None


def is_false(v):
    return v is False


def is_num(v):
    return isinstance(v, (int, float))  # admits bool


def num(v):
    if isinstance(v, bool):
        return 1.0 if v else 0.0
    if isinstance(v, (int, float)):
        return float(v)
    return 0.0


def get(o, k):
    return o.get(k) if isinstance(o, dict) else None


def getattr_d(obj, attr, default=None):
    if isinstance(obj, dict):
        return obj.get(attr, default)
    return default


def copy(v):
    return json.loads(json.dumps(v))


def strip_nulls(v):
    if v is None:
        return None
    if isinstance(v, list):
        return [strip_nulls(x) for x in v]
    if isinstance(v, dict):
        return {k: strip_nulls(x) for k, x in v.items() if x is not None}
    return v


def special(v):
    if isinstance(v, dict):
        s = v.get("__special__")
        if s == "nan":
            return True, float("nan")
        if s == "inf":
            return True, float("inf")
        if s == "-inf":
            return True, float("-inf")
    return False, 0.0


def typename(v):
    if v is None:
        return "NoneType"
    if isinstance(v, bool):
        return "bool"
    if isinstance(v, (int, float)):
        return "int"
    if isinstance(v, str):
        return "str"
    if isinstance(v, list):
        return "list"
    if isinstance(v, dict):
        return "dict"
    return "object"


# ---- string helpers ----
def is_ws(c):
    return c in " \t\n\r\v\f" or 0x1c <= ord(c) <= 0x1f


def lstrip(s):
    if s is None:
        s = ""
    a = 0
    while a < len(s) and is_ws(s[a]):
        a += 1
    return s[a:]


def startswith(s, p):
    return s is not None and s.startswith(p)


def pystr(v):
    if v is None:
        return "None"
    if isinstance(v, bool):
        return "True" if v else "False"
    if isinstance(v, (int, float)):
        d = float(v)
        if math.isfinite(d) and d == math.floor(d) and abs(d) < 1e16:
            return "%.0f" % d
        return "%.17g" % d
    if isinstance(v, str):
        return v
    return json.dumps(v, ensure_ascii=False, separators=(",", ":"))


def repr_str(s):
    if s is None:
        s = ""
    has_sq = "'" in s
    has_dq = '"' in s
    q = '"' if (has_sq and not has_dq) else "'"
    out = [q]
    for c in s:
        if c == "\\":
            out.append("\\\\")
        elif c == "\n":
            out.append("\\n")
        elif c == "\r":
            out.append("\\r")
        elif c == "\t":
            out.append("\\t")
        elif c == q:
            out.append("\\" + c)
        elif ord(c) < 0x20:
            out.append("\\x%02x" % ord(c))
        else:
            out.append(c)
    out.append(q)
    return "".join(out)


def cv_field_repr(v, is_tuple):
    if v is None:
        return "None"
    if isinstance(v, bool):
        return "True" if v else "False"
    if isinstance(v, str):
        return repr_str(v)
    if isinstance(v, list):
        items = [cv_field_repr(x, False) for x in v]
        body = ", ".join(items)
        if is_tuple:
            if len(v) == 1:
                body += ","
            return "(" + body + ")"
        return "[" + body + "]"
    return "None"


def join_str(arr, sep):
    if isinstance(arr, list):
        return sep.join((x if isinstance(x, str) else "") for x in arr)
    return ""


def py_eq(a, b):
    an = a is None
    bn = b is None
    if an or bn:
        return an and bn
    ab = isinstance(a, bool)
    bb = isinstance(b, bool)
    anum = isinstance(a, (int, float))
    bnum = isinstance(b, (int, float))
    if (ab or anum) and (bb or bnum):
        return num(a) == num(b)
    if isinstance(a, str) and isinstance(b, str):
        return a == b
    if isinstance(a, list) and isinstance(b, list):
        if len(a) != len(b):
            return False
        return all(py_eq(x, y) for x, y in zip(a, b))
    return False


# ---- exact format(v, ".2f") transcription ----
def fmt_fixed(v, prec):
    if math.isnan(v):
        return "nan"
    if math.isinf(v):
        return "-inf" if v < 0 else "inf"
    bits = struct.unpack("<Q", struct.pack("<d", v))[0]
    neg = bits >> 63
    mant = bits & ((1 << 52) - 1)
    expf = (bits >> 52) & 0x7FF
    if expf == 0:
        e = -1074
    else:
        mant |= (1 << 52)
        e = expf - 1023 - 52
    scale = 100 if prec == 2 else (10 if prec == 1 else 1)
    M = mant * scale
    if e >= 0:
        Q = M << e
    else:
        shift = -e
        if shift >= 61:
            Q = 0
        else:
            base = 1 << shift
            Q = M >> shift
            rem = M & (base - 1)
            twice = rem << 1
            if twice > base or (twice == base and (Q & 1)):
                Q += 1
    ip = Q // scale
    fp = Q % scale
    sign = "-" if neg else ""
    if prec == 0:
        return "%s%d" % (sign, ip)
    return "%s%d.%0*d" % (sign, ip, prec, fp)


def fmt_usd_str(d):
    fixed = fmt_fixed(d, 2)
    if math.isnan(d) or math.isinf(d):
        return "$" + fixed
    sign = ""
    body = fixed
    if fixed[0] == "-":
        sign = "-"
        body = fixed[1:]
    dot = body.find(".")
    ilen = dot if dot >= 0 else len(body)
    first = ilen % 3
    if first == 0:
        first = 3
    out = "$" + sign + body[:first]
    i = first
    while i < ilen:
        out += "," + body[i:i + 3]
        i += 3
    if dot >= 0:
        out += body[dot:]
    return out


def is_finite_num_core(v):
    sp, val = special(v)
    if sp:
        return math.isfinite(val)
    if isinstance(v, bool):
        return False
    if isinstance(v, (int, float)):
        return math.isfinite(v)
    return False


def fmt_usd_core(d):
    sp, val = special(d)
    if sp:
        v = val
    elif is_num(d):
        v = num(d)
    else:
        if isinstance(d, str):
            return mkraise("ValueError", "Unknown format code 'f' for object of type 'str'")
        return mkraise("TypeError", "unsupported format string passed to %s.__format__" % typename(d))
    return fmt_usd_str(v)


def clamp_pct(p):
    if p < 0.0:
        p = 0.0
    if p > 100.0:
        p = 100.0
    return p


def window(label, used_percent, detail):
    return {"label": label, "used_percent": used_percent, "reset_at": None,
            "detail": copy(detail) if detail is not None else None, "scope": "account"}


def cv_carrier(logged_in, balance_lines, identity_line, topup_url, depleted):
    return {"logged_in": logged_in,
            "balance_lines": balance_lines if balance_lines is not None else [],
            "identity_line": copy(identity_line) if identity_line is not None else None,
            "topup_url": copy(topup_url) if topup_url is not None else None,
            "depleted": depleted}


# ---- live message constants ----
DEPLETED = "Status: access depleted \u2014 top up to restore"
DEV_TAIL = "(dev fixture \u2014 HERMES_DEV_CREDITS_FIXTURE)"
LOG_TOPUP = "credits \u25b8 /topup portal fetch failed (fail-open)"
TOPUP_VIA = "Top up via /topup"
OR_RUN = "(or run /topup)"
TOPUP_PREFIX = "Top up: "
RENEWS_PREFIX = "Renews: "
ROLLOVER_PREFIX = "Rollover: "
TOPUP_CREDITS = "Top-up credits: "
SUBSCRIPTION = "Subscription"
OF = " of "
LEFT = " left"
TOPPING_UP = "Topping up as "
ORG_PREFIX = "org "
JOIN_SEP = " / "


# ---- universal-seam dispatcher (return-value raise-spec propagation) ----
class Disp:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def call(self, name, args):
        entry = self.io.get(name, MISSING)
        present_e = entry is not MISSING
        exhausted = False
        result = None
        if present_e:
            if isinstance(entry, list):
                idx = self.cursors.get(name, 0)
                if idx >= len(entry):
                    exhausted = True
                else:
                    result = entry[idx]
                    self.cursors[name] = idx + 1
            else:
                result = entry
        failclosed = (not present_e) or exhausted
        actual = result
        if not failclosed and isinstance(actual, dict) and actual.get("__script__") is True:
            actual = actual.get("ret")
        if failclosed:
            retnode = mkraise("RuntimeError", "no script for " + name)
        else:
            retnode = copy(actual) if actual is not None else None
        self.trace.append({"ev": name, "args": strip_nulls(args), "ret": copy(retnode)})
        if failclosed:
            return retnode
        if actual is None:
            return None
        return retnode


def seam_uwr(d, gauges, paused, keep, cta, include_cta):
    a = {"gauges": copy(gauges) if gauges is not None else None,
         "paused_reason": copy(paused) if paused is not None else None,
         "keep_going_on_credits": copy(keep) if keep is not None else None}
    if include_cta:
        a["cta"] = cta
    return d.call("usage_window_rows", a)


def seam_snapshot(d, windows, details, tail, source, plan, uw, include_uw):
    a = {"windows": windows if windows is not None else [],
         "details": details if details is not None else [],
         "tail": tail if tail is not None else [],
         "source": source,
         "plan": copy(plan) if plan is not None else None}
    if include_uw:
        a["usage_window"] = uw
    return d.call("nous_snapshot", a)


def seam_topup(d, account):
    return d.call("nous_portal_topup_url", {"account_info": copy(account) if account is not None else None})


def seam_renews(d, value):
    return d.call("format_renews", {"value": copy(value) if value is not None else None})


def seam_render(d, snapshot, markdown):
    return d.call("render_account_usage_lines",
                  {"snapshot": copy(snapshot) if snapshot is not None else None, "markdown": markdown})


def seam_fetch(d, timeout):
    return d.call("fetch_portal_account", {"timeout": timeout})


def seam_logged_in(d):
    return d.call("nous_logged_in", {})


def seam_credits_view(d, markdown, timeout, credits_state, nous_route):
    return d.call("nous_credits_view",
                  {"markdown": markdown, "timeout": timeout,
                   "credits_state": copy(credits_state) if credits_state is not None else None,
                   "nous_route": nous_route})


def log_exc(d, fmt):
    return d.call("logger.debug", {"fmt": fmt, "args": [], "exc_info": True})


def extend(dst, src):
    if isinstance(src, list):
        for x in src:
            dst.append(copy(x))


# ---- build_nous_credits_snapshot core ----
def build_snapshot_core(d, account, cta):
    if not present(account) or not truth(getattr_d(account, "logged_in")):
        return None
    access = getattr_d(account, "paid_service_access_info")
    sub = getattr_d(account, "subscription")
    uw = getattr_d(account, "usage_window")
    windows = []
    details = []
    window_lines = []
    if present(uw):
        r = seam_uwr(d, getattr_d(uw, "gauges"), getattr_d(uw, "paused_reason"),
                     getattr_d(uw, "keep_going_on_credits"), cta, True)
        if is_raise(r):
            return None if caught_by(kind(r), ATE) else r
        w = r[0] if isinstance(r, list) and len(r) > 0 else None
        wl = r[1] if isinstance(r, list) and len(r) > 1 else None
        windows = copy(w) if w is not None else []
        window_lines = copy(wl) if wl is not None else []
    if truth(windows):
        extend(details, window_lines)
        topup = getattr_d(access, "purchased_credits_remaining") if present(access) else None
        if is_finite_num_core(topup):
            f = fmt_usd_core(topup)
            details.append(TOPUP_CREDITS + f)
        if is_false(getattr_d(account, "paid_service_access")) and not truth(window_lines):
            details.append(DEPLETED)
        period_end = getattr_d(sub, "current_period_end") if present(sub) else None
        if present(sub) and truth(period_end):
            fr = seam_renews(d, period_end)
            if is_raise(fr):
                return None if caught_by(kind(fr), ATE) else fr
            eff = fr if truth(fr) else period_end
            details.append(RENEWS_PREFIX + pystr(eff))
        tail = [TOPUP_VIA] if cta else []
        plan = getattr_d(sub, "plan") if present(sub) else None
        r = seam_snapshot(d, windows, details, tail, "portal-account", plan, True, True)
        if is_raise(r) and caught_by(kind(r), ATE):
            return None
        return r
    if present(sub):
        cap = getattr_d(sub, "monthly_credits")
        sub_remaining = getattr_d(sub, "credits_remaining")
        if (is_finite_num_core(cap) and num(cap) > 0.0 and
                is_finite_num_core(sub_remaining) and num(sub_remaining) <= num(cap)):
            p = clamp_pct((num(cap) - num(sub_remaining)) / num(cap) * 100.0)
            f1 = fmt_usd_core(sub_remaining)
            f2 = fmt_usd_core(cap)
            det = f1 + OF + f2 + LEFT
            windows.append(window(SUBSCRIPTION, p, det))
    if present(access):
        for attr, label in (("subscription_credits_remaining", "Subscription credits"),
                            ("purchased_credits_remaining", "Top-up credits"),
                            ("total_usable_credits", "Total usable")):
            value = getattr_d(access, attr)
            if is_finite_num_core(value):
                details.append("%s: %s" % (label, fmt_usd_core(value)))
    if present(sub):
        rollover = getattr_d(sub, "rollover_credits")
        if is_finite_num_core(rollover) and num(rollover) > 0.0:
            details.append(ROLLOVER_PREFIX + fmt_usd_core(rollover))
        period_end = getattr_d(sub, "current_period_end")
        if truth(period_end):
            details.append(RENEWS_PREFIX + pystr(period_end))
    if is_false(getattr_d(account, "paid_service_access")):
        details.append(DEPLETED)
    tail = []
    if cta:
        url = seam_topup(d, account)
        if is_raise(url):
            return None if caught_by(kind(url), ATE) else url
        tail.append(TOPUP_PREFIX + pystr(url))
        tail.append(OR_RUN)
    plan = getattr_d(sub, "plan") if present(sub) else None
    r = seam_snapshot(d, windows, details, tail, "portal-account", plan, False, False)
    if is_raise(r) and caught_by(kind(r), ATE):
        return None
    return r


# ---- _snapshot_from_credits_state core ----
def snapshot_from_state_core(d, state):
    if not present(state):
        return None
    windows = []
    details = []
    window_lines = []
    if truth(getattr_d(state, "is_usage_window")):
        if not isinstance(state, dict) or "gauges" not in state:
            return None
        gauges = state["gauges"]
        if "paused_reason" not in state:
            return None
        paused = state["paused_reason"]
        keep = getattr_d(state, "keep_going_on_credits")
        r = seam_uwr(d, gauges, paused, keep, True, False)
        if is_raise(r):
            return None if caught_by(kind(r), ATE) else r
        w = r[0] if isinstance(r, list) and len(r) > 0 else None
        wl = r[1] if isinstance(r, list) and len(r) > 1 else None
        windows = copy(w) if w is not None else []
        window_lines = copy(wl) if wl is not None else []
        extend(details, window_lines)
        purchased = getattr_d(state, "purchased_usd")
        if truth(purchased):
            details.append("Top-up credits: $" + pystr(purchased))
        if is_false(getattr_d(state, "paid_access")) and not truth(window_lines):
            details.append(DEPLETED)
        tail = [DEV_TAIL]
        r = seam_snapshot(d, windows, details, tail, "dev-fixture", None, True, True)
        if is_raise(r) and caught_by(kind(r), ATE):
            return None
        return r
    uf = getattr_d(state, "used_fraction")
    sub_usd = getattr_d(state, "subscription_usd")
    cap_usd = getattr_d(state, "subscription_limit_usd")
    if is_num(uf) and math.isfinite(num(uf)):
        p = clamp_pct(num(uf) * 100.0)
        detail = None
        if truth(sub_usd) and truth(cap_usd):
            detail = "$%s of $%s left" % (pystr(sub_usd), pystr(cap_usd))
        windows.append(window(SUBSCRIPTION, p, detail))
    purchased = getattr_d(state, "purchased_usd")
    remaining = getattr_d(state, "remaining_usd")
    for value, label in ((sub_usd, "Subscription credits"), (purchased, "Top-up credits"),
                         (remaining, "Total usable")):
        if truth(value):
            details.append("%s: $%s" % (label, pystr(value)))
    if is_false(getattr_d(state, "paid_access")):
        details.append(DEPLETED)
    tail = [DEV_TAIL]
    r = seam_snapshot(d, windows, details, tail, "dev-fixture", None, False, False)
    if is_raise(r) and caught_by(kind(r), ATE):
        return None
    return r


# ---- build_credits_view core ----
def build_credits_view_core(d, markdown, timeout):
    not_logged_in = cv_carrier(False, None, None, None, False)
    li = seam_logged_in(d)
    if is_raise(li):
        return li
    if not truth(li):
        return not_logged_in
    account = seam_fetch(d, timeout)
    if is_raise(account):
        if not is_exception(kind(account)):
            return account
        lg = log_exc(d, LOG_TOPUP)
        if is_raise(lg):
            return lg
        return not_logged_in
    if not present(account) or not truth(getattr_d(account, "logged_in")):
        return not_logged_in
    snapshot = build_snapshot_core(d, account, True)
    if is_raise(snapshot):
        return snapshot
    lines = seam_render(d, snapshot, markdown)
    if is_raise(lines):
        return lines
    balance = []
    if isinstance(lines, list):
        for ln in lines:
            if not isinstance(ln, str):
                return mkraise("AttributeError", "'%s' object has no attribute 'lstrip'" % typename(ln))
            ls = lstrip(ln)
            drop = startswith(ls, "Top up:") or startswith(ls, "Top up via") or startswith(ls, "(or run")
            if not drop:
                balance.append(copy(ln))
    who = []
    email = getattr_d(account, "email")
    if truth(email):
        who.append(pystr(email))
    org = getattr_d(account, "org_name")
    if truth(org):
        who.append(ORG_PREFIX + pystr(org))
    identity = None
    if truth(who):
        identity = TOPPING_UP + join_str(who, JOIN_SEP)
    url = seam_topup(d, account)
    if is_raise(url):
        return url
    depleted = is_false(getattr_d(account, "paid_service_access"))
    return cv_carrier(True, balance, identity, url, depleted)


# ---- entries ----
def entry(func, inp, d):
    g = inp.get
    if func == "_fmt_usd":
        return fmt_usd_core(g("d"))
    if func == "_is_finite_num":
        return is_finite_num_core(g("v"))
    if func == "build_nous_credits_snapshot":
        return build_snapshot_core(d, g("account"), truth(g("cta")))
    if func == "nous_credits_lines":
        r = seam_credits_view(d, truth(g("markdown")), num(g("timeout")),
                              g("credits_state"), truth(g("nous_route")))
        if is_raise(r):
            return r
        lines = r[0] if isinstance(r, list) and len(r) > 0 else None
        return copy(lines) if lines is not None else None
    if func == "_snapshot_from_credits_state":
        return snapshot_from_state_core(d, g("state"))
    if func == "build_credits_view":
        return build_credits_view_core(d, truth(g("markdown")), num(g("timeout")))
    if func.startswith("CreditsView."):
        m = func[len("CreditsView."):]
        if m == "__init__":
            kw = g("kw") or {}
            return {"logged_in": copy(kw.get("logged_in")) if "logged_in" in kw else None,
                    "balance_lines": copy(kw["balance_lines"]) if "balance_lines" in kw else [],
                    "identity_line": copy(kw["identity_line"]) if "identity_line" in kw else None,
                    "topup_url": copy(kw["topup_url"]) if "topup_url" in kw else None,
                    "depleted": copy(kw["depleted"]) if "depleted" in kw else False}
        if m == "__repr__":
            s = g("self") or {}
            return "CreditsView(logged_in=%s, balance_lines=%s, identity_line=%s, topup_url=%s, depleted=%s)" % (
                cv_field_repr(s.get("logged_in"), False),
                cv_field_repr(s.get("balance_lines"), True),
                cv_field_repr(s.get("identity_line"), False),
                cv_field_repr(s.get("topup_url"), False),
                cv_field_repr(s.get("depleted"), False))
        if m == "__eq__":
            s = g("self") or {}
            other = g("other")
            ocv = other.get("__credits_view__") if isinstance(other, dict) else None
            if ocv is None:
                return "NotImplemented"
            for f in ("logged_in", "balance_lines", "identity_line", "topup_url", "depleted"):
                if not py_eq(s.get(f), ocv.get(f)):
                    return False
            return True
        if m == "__setattr__":
            nm = g("name")
            return mkraise("FrozenInstanceError", "cannot assign to field %s" % repr_str(nm if nm is not None else ""))
        if m == "__delattr__":
            nm = g("name")
            return mkraise("FrozenInstanceError", "cannot delete field %s" % repr_str(nm if nm is not None else ""))
    return mkraise("RuntimeError", "unknown func")


def run(func, inp):
    d = Disp(inp.get("io"))
    ret = entry(func, inp, d)
    return {"func": func, "in": inp, "ret": ret if ret is not None else None,
            "trace": d.trace, "st": {}}