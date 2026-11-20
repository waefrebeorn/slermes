#!/usr/bin/env python3
"""Oracle for relay_auth_flow3 — standalone FLOW port of the LIVE quarry module
gateway/relay/auth.py (the C port's "relay_auth.py").

HONEST PoP SPLIT (CRLF-aware grep at authoring time): the live module has
exactly three AST defs — sign, make_token, make_upgrade_token — and all three
are ALREADY claimed by older stub ports (relay_auth.c / relay_auth_helpers2.c).
Five more claims (_delivery_payload, _hmac_hex, verify_signature, verify_token,
verify_delivery_signature) are STALE: upstream removed those defs. The genuine
unclaimed-remainder count is therefore 0. Per the remainder-scoping rule this
oracle ports the WHOLE live module as a standalone FLOW differential check;
no non-existent gap is claimed and the older claims stay in force.

Drives the LIVE defs AST-extracted from the quarry. The three effectful
collaborators (time.time, hmac.new, base64.urlsafe_b64encode) are monkeypatched
with recorders that fire the SAME universal seam the C harness feeds: a MISSING
script key fail-closes with RuntimeError on both sides, a present JSON null is
a valid None, an array script yields one result PER CALL (last repeats). A
scripted {"__raise__": cls, "msg": m} raises a REAL builtin of that class
(full py3.14 parent hierarchy, including the BaseException trio), so
SystemExit/KeyboardInterrupt/GeneratorExit/BaseException propagate exactly as
the C raise dict does. Args/ret are DEEP-COPIED into the trace at call time.

Input: stdin JSON lines from t_port_relay_auth_flow3:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...]}}
Summary: ORACLE relay_auth_flow3: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.

--self-test  drives every fixture LIVE (no C) and reports binding/script gaps.
--shadow     diffs a literal Python MIRROR of the C port against the LIVE drive
             over every fixture (the shadow-diff; must print 0 diffs).
--gate-proxy feeds the mirror's {func,in,out} envelopes through the REAL main()
             comparison path (code-only stand-in for run_one_oracle.sh).
"""
import ast
import copy
import io
import json
import math
import os
import sys
import types

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

from pathlib import Path

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "relay_auth_flow3_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "relay", "auth.py")

# harness func -> live python name (all three are module-level defs)
PY = {
    "sign": "sign",
    "make_token": "make_token",
    "make_upgrade_token": "make_upgrade_token",
}
WANTED = set(PY.values())
SEAM_NAMES = ["time_time", "hmac_hexdigest", "b64url_encode"]

CUR = {"trace": [], "scripts": {}, "idx": {}}
NS = {}
REAL = {}


# ── full builtin exception hierarchy (py3.14) ──

EXC_MAP = {
    "BaseException": BaseException, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "StopIteration": StopIteration, "StopAsyncIteration": StopAsyncIteration,
    "ArithmeticError": ArithmeticError, "FloatingPointError": FloatingPointError,
    "OverflowError": OverflowError, "ZeroDivisionError": ZeroDivisionError,
    "AssertionError": AssertionError, "AttributeError": AttributeError,
    "BufferError": BufferError, "EOFError": EOFError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "ZipImportError": ImportError, "LookupError": LookupError,
    "IndexError": IndexError, "KeyError": KeyError,
    "MemoryError": MemoryError, "NameError": NameError,
    "UnboundLocalError": UnboundLocalError, "OSError": OSError,
    "BlockingIOError": BlockingIOError, "ChildProcessError": ChildProcessError,
    "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "FileExistsError": FileExistsError,
    "FileNotFoundError": FileNotFoundError, "InterruptedError": InterruptedError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
    "TimeoutError": TimeoutError, "ReferenceError": ReferenceError,
    "RuntimeError": RuntimeError, "NotImplementedError": NotImplementedError,
    "RecursionError": RecursionError, "PythonFinalizationError": RuntimeError,
    "SyntaxError": SyntaxError, "IndentationError": IndentationError,
    "TabError": TabError, "_IncompleteInputError": SyntaxError,
    "SystemError": SystemError, "TypeError": TypeError, "ValueError": ValueError,
    "UnicodeError": UnicodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeTranslateError": UnicodeTranslateError,
    "JSONDecodeError": ValueError,
    "Warning": Warning, "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning, "SyntaxWarning": SyntaxWarning,
    "RuntimeWarning": RuntimeWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
    "ResourceWarning": ResourceWarning,
    "BaseExceptionGroup": BaseExceptionGroup, "ExceptionGroup": ExceptionGroup,
}


def make_exc(name, msg):
    """Build a REAL builtin of the scripted class. The Unicode*Error trio and
    the ExceptionGroup pair need constructor bypasses: UnicodeEncodeError(msg)
    raises TypeError (it takes 5 args), and a plain cls.__new__(cls) is unsafe
    on py3.14 when __init__ is overridden. """
    cls = EXC_MAP.get(name)
    if cls is None:
        cls = type(name, (Exception,), {})
    if name in ("UnicodeEncodeError", "UnicodeDecodeError", "UnicodeTranslateError"):
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    if name in ("ExceptionGroup", "BaseExceptionGroup"):
        return cls(msg, [ValueError("inner")])
    return cls(msg)


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r["__raise__"], r.get("msg", ""))
    return r


def raise_spec(name, msg=""):
    return {"__raise__": name, "msg": msg}


# ── seam recorder ──

def seam(name, args):
    args = copy.deepcopy({k: v for k, v in args.items() if v is not None})
    if name not in CUR["scripts"]:
        r = raise_spec("RuntimeError", "unscripted seam: " + name)
    else:
        sc = CUR["scripts"][name]
        if isinstance(sc, dict) and "__arr__" in sc:
            r = sc["__arr__"]
        elif isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": copy.deepcopy(r)})
    return maybe_raise(r)


def is_raise(r):
    return isinstance(r, dict) and "__raise__" in r


def exc_to_spec(ex):
    """Project a raised exception back to the SCRIPTED raise dict when it came
    from a seam. Python reformats some builtin messages (KeyError('k') ->
    "'k'", UnicodeEncodeError -> "", ExceptionGroup -> "e (1 sub-exception)"),
    while the C port propagates the scripted dict verbatim — the fixture is a
    raise-SPEC differential, so compare the spec, not str(ex). A genuinely
    live-raised exception (no matching scripted event) still uses str(ex)."""
    name = type(ex).__name__
    for ev in reversed(CUR["trace"]):
        r = ev.get("ret")
        if is_raise(r) and r.get("__raise__") == name:
            return copy.deepcopy(r)
    return {"__raise__": name, "msg": str(ex)}


# ── extraction: AST from the live quarry, no heavy imports ──

def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {"__name__": "relay_auth_flow3_extracted"}
    tree = ast.parse(src)
    # Module constant evaluated at make_upgrade_token def-exec time.
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "_DEFAULT_UPGRADE_TTL_SECONDS"
                for t in node.targets):
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    if "_DEFAULT_UPGRADE_TTL_SECONDS" not in ns:
        print("FATAL missing constant: _DEFAULT_UPGRADE_TTL_SECONDS")
        sys.exit(2)
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    global NS, REAL
    NS = ns
    REAL = dict(ns)
    return ns


# ── recording doubles for the three seamed live callables ──

class _HmacDouble:
    def __init__(self, r):
        self.r = r

    def hexdigest(self):
        return self.r


class _FakeHmac:
    def new(self, key, msg, digestmod=None):
        r = seam("hmac_hexdigest", {"secret": key.decode("utf-8"),
                                   "payload": msg.decode("utf-8")})
        return _HmacDouble(r)


class _FakeTime:
    def time(self):
        return seam("time_time", {})


class _B64Double:
    def __init__(self, r):
        self.r = r

    def decode(self, encoding=None):
        return self.r


class _FakeBase64:
    def urlsafe_b64encode(self, raw):
        return _B64Double(seam("b64url_encode", {"raw": raw.decode("ascii")}))


def install_stubs(ns):
    ns["time"] = _FakeTime()
    ns["hmac"] = _FakeHmac()
    ns["base64"] = _FakeBase64()
    ns["hashlib"] = types.SimpleNamespace(sha256=object())


def reset_case(inp):
    CUR["trace"] = []
    CUR["scripts"] = inp.get("seams") or {}
    CUR["idx"] = {}
    # Restore the REAL target defs before installing per-case stubs (a prior
    # case's recorder must never leak into a later case's composed call).
    for k, v in REAL.items():
        NS[k] = v
    install_stubs(NS)


def run_live(func, inp):
    reset_case(inp)
    try:
        if func == "sign":
            r = NS["sign"](inp.get("payload", ""), inp.get("secret", ""))
        elif func == "make_token":
            if "ttl_seconds" in inp:
                r = NS["make_token"](inp.get("payload", ""), inp.get("secret", ""),
                                     inp["ttl_seconds"])
            else:
                r = NS["make_token"](inp.get("payload", ""), inp.get("secret", ""))
        elif func == "make_upgrade_token":
            if "ttl_seconds" in inp:
                r = NS["make_upgrade_token"](inp.get("payload", ""), inp.get("secret", ""),
                                             inp["ttl_seconds"])
            else:
                r = NS["make_upgrade_token"](inp.get("payload", ""), inp.get("secret", ""))
        else:
            raise KeyError(func)
        return {"ret": r, "trace": CUR["trace"]}, None
    except BaseException as ex:
        return ({"ret": exc_to_spec(ex), "trace": CUR["trace"]}, ex)


# ── canonical comparison (parsed objects, integral floats reconciled) ──

def canon(v):
    if v is None:
        return ("null",)
    if isinstance(v, bool):
        return ("bool", v)
    if isinstance(v, (int, float)):
        f = float(v)
        return ("num", int(v)) if math.isfinite(f) and f.is_integer() else ("num", f)
    if isinstance(v, str):
        return ("str", v)
    if isinstance(v, list):
        return ("list", tuple(canon(x) for x in v))
    if isinstance(v, dict):
        return ("obj", tuple(sorted((str(k), canon(val)) for k, val in v.items())))
    return ("other", repr(v))


# ── fixture + self-test ──

def fixture():
    return _REPO / "tests" / "fixtures" / "relay_auth_flow3.jsonl"


def load_cases():
    p = fixture()
    if not p.exists():
        return []
    return [json.loads(l) for l in p.read_text(encoding="utf-8").splitlines() if l.strip()]


def check_func_set(cases):
    ff = {c["func"] for c in cases}
    if ff != set(PY):
        print("FATAL func set drift: fixture-only=%s map-only=%s"
              % (sorted(ff - set(PY)), sorted(set(PY) - ff)))
        sys.exit(2)
    counts = {}
    for c in cases:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("FATAL <5 cases for: %s" % thin)
        sys.exit(2)


def self_test():
    extract()
    cases = load_cases()
    check_func_set(cases)
    bad = 0
    for c in cases:
        env, ex = run_live(c["func"], c["in"])
        ret = env["ret"]
        if isinstance(ret, dict) and ret.get("__raise__") == "RuntimeError" and \
                "unscripted seam" in ret.get("msg", ""):
            print("SELFTEST SCRIPTGAP %s: %s" % (c["func"], ret["msg"]))
            bad += 1
        elif isinstance(ex, AttributeError) and any(
                ("'%s'" % n) in str(ex) for n in list(PY) + SEAM_NAMES):
            print("SELFTEST BINDING %s: %s" % (c["func"], ex))
            bad += 1
        elif isinstance(ex, NameError):
            print("SELFTEST NAMEERROR %s: %s" % (c["func"], ex))
            bad += 1
        elif isinstance(ex, TypeError) and "relay_auth_flow3" not in str(ex):
            # A TypeError family naming a ported call site is a binding gap;
            # a scripted TypeError (msg "t"/"hm"/...) is expected.
            if any(n in str(ex) for n in ("argument", "positional", "missing")):
                print("SELFTEST BINDING-TYPE %s: %s" % (c["func"], ex))
                bad += 1
    print("SELFTEST relay_auth_flow3: %d cases, %d binding/script errors" % (len(cases), bad))
    sys.exit(1 if bad else 0)


# ── shadow mirror: literal transcription of the C ra3_* cores ──

def shadow_drive(func, inp):
    """Mirror of ra3_sign / ra3_make_token / ra3_make_upgrade_token. Evaluate
    each seam ONE AT A TIME (never in a tuple literal) so an early raise stops
    the later seams exactly as the C call sequence does."""
    reset_case(inp)
    payload = inp.get("payload", "")
    secret = inp.get("secret", "")

    def ra3_sign(p, s):
        # C builds {secret, payload}; order matters only for raw JSON, canon
        # sorts keys, but emit in C order for trace parity.
        return seam("hmac_hexdigest", {"secret": s, "payload": p})

    def ra3_make_token(p, s, ttl):
        if ttl > 0:
            tm = seam("time_time", {})
            if is_raise(tm):
                maybe_raise(tm)              # propagate, no later seam fires
            if isinstance(tm, bool) or not isinstance(tm, (int, float)):
                return raise_spec("TypeError",
                                  "'str' object cannot be interpreted as an integer")
            exp_i = math.trunc(tm) + math.trunc(ttl)
            exp = str(exp_i)
        else:
            exp = "0"
        signed = "%s:%s" % (p, exp)
        sig = ra3_sign(signed, s)
        if is_raise(sig):
            maybe_raise(sig)
        if not isinstance(sig, str):
            return raise_spec("TypeError", "sequence item 1: expected str instance")
        raw = "%s:%s" % (signed, sig)        # signed, NOT bare payload
        enc = seam("b64url_encode", {"raw": raw})
        if is_raise(enc):
            maybe_raise(enc)
        if enc is None:
            return None
        if not isinstance(enc, str):
            return raise_spec("TypeError", "sequence item 1: expected str instance")
        return enc.rstrip("=")               # .decode("ascii").rstrip("=")

    if func == "sign":
        ret = ra3_sign(payload, secret)
    elif func == "make_token":
        ret = ra3_make_token(payload, secret,
                             inp.get("ttl_seconds", 0))
    elif func == "make_upgrade_token":
        ret = ra3_make_token(payload, secret,
                             inp.get("ttl_seconds", 300))
    else:
        raise KeyError(func)
    return ret


def shadow_envelope(func, inp):
    try:
        ret = shadow_drive(func, inp)
    except BaseException as ex:
        ret = exc_to_spec(ex)
    return {"ret": ret, "trace": CUR["trace"]}


def shadow_mode():
    extract()
    cases = load_cases()
    check_func_set(cases)
    diffs = 0
    for c in cases:
        want, _ = run_live(c["func"], c["in"])
        try:
            got = shadow_envelope(c["func"], c["in"])
        except BaseException as ex:
            got = {"ret": {"__raise__": type(ex).__name__, "msg": str(ex)}, "trace": []}
        if canon(want) != canon(got):
            diffs += 1
            print("SHADOW DIFF %s" % c["func"])
            print("  MIRROR: %s" % json.dumps(got, ensure_ascii=False))
            print("  LIVE  : %s" % json.dumps(want, ensure_ascii=False))
    print("SHADOW relay_auth_flow3: %d cases, %d diffs" % (len(cases), diffs))
    sys.exit(1 if diffs else 0)


def compare_lines(lines):
    extract()
    cases = 0
    mism = 0
    for line in lines:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        want, _ = run_live(func, inp)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE relay_auth_flow3: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE relay_auth_flow3: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


def gate_proxy():
    """Code-only stand-in: build the mirror's harness-shaped envelopes and run
    them through the SAME comparison path main() uses."""
    cases = load_cases()
    check_func_set(cases)
    lines = []
    for c in cases:
        env = shadow_envelope(c["func"], c["in"])
        lines.append(json.dumps({"func": c["func"], "in": c["in"], "out": env},
                                ensure_ascii=False))
    compare_lines(lines)


def main():
    if "--self-test" in sys.argv:
        self_test()
        return
    if "--shadow" in sys.argv:
        shadow_mode()
        return
    if "--gate-proxy" in sys.argv:
        gate_proxy()
        return
    compare_lines(sys.stdin)


if __name__ == "__main__":
    main()
