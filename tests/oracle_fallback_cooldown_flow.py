#!/usr/bin/env python3
"""Oracle for the FLOW + leaf port of agent/fallback_cooldown.py
(src/agent/fallback_cooldown_flow.c, fc_* entries) plus its UNCLAIMED leaf
neighborhood: hermes_cli/route_identity.py (provider_owns_route,
drop_stale_model_route, drop_credentials_of_other_route) and
agent/error_classifier.py (_error_obj, _json_dict,
_usage_window_display_message, _has_usage_limit_transient_signal,
_rate_limit_reset_seconds).

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"] with scripted universal-seam stubs, and
compares the ordered trace, the return, and the state projection. Live defs
are AST-extracted (py3.14 quarry venv); claimed siblings
(_parse_absolute_timestamp, normalize_route_base_url,
normalize_model_for_provider) and all I/O are recording seams fed the SAME
scripts the C harness feeds. Ported siblings stay REAL so composition is
exercised (switch_deferred -> _provider_reset_delay; _on_primary_route ->
ident; _arm -> _on_primary_route + _provider_reset_delay; drop_stale ->
provider_owns_route; the reset-signal leaves -> _error_obj).

Fail-closed: 0 cases => exit 1; a missing/exhausted script RETURNS a Raise
spec (recorded before raising) so the trace matches the C dispatcher.
BaseException caught per case -> __raise__ spec, st={}. The one
`except Exception` site (switch_deferred_by_reset) passes the BaseException
trio (+CancelledError) through.
Summary: 'ORACLE fallback_cooldown_flow: N cases, M mismatches'.

Domain (documented in the port header): str()-positions carry strings /
integral numbers; strip/lower ASCII; reason is a FailoverReason value string
or null; api_error is an object double with an explicit "str" projection;
pdef scripts are null/non-empty dict; response_headers is null/dict/non-dict;
_json_dict excludes NaN/Infinity and lone surrogates; backoff_count is an int
(or a raising non-numeric carrier).
"""
import ast
import copy
import enum
import json
import math
import os
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
if not (QUARRY / "agent" / "fallback_cooldown.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))

FC_SRC = str(QUARRY / "agent" / "fallback_cooldown.py")
RI_SRC = str(QUARRY / "hermes_cli" / "route_identity.py")
EC_SRC = str(QUARRY / "agent" / "error_classifier.py")

ANN_FLAG = __future__.annotations.compiler_flag

# live target names (the nested ident closure included)
FUNCS = [
    "_provider_reset_delay", "switch_deferred_by_reset", "_on_primary_route",
    "ident", "_arm_rate_limit_cooldown", "_mark_entitlement_rejected_model",
    "_is_entitlement_rejected",
    "provider_owns_route", "drop_stale_model_route",
    "drop_credentials_of_other_route",
    "_error_obj", "_json_dict", "_usage_window_display_message",
    "_has_usage_limit_transient_signal", "_rate_limit_reset_seconds",
]

# module-global claimed sibling (live name -> seam name)
SEAM_GLOBALS = {
    "normalize_route_base_url": "normalize_route_base_url",
}
# in-function imports: module -> {attr: seam name} (None = real constant)
SEAM_IMPORTS = {
    "agent.credential_pool": {"_parse_absolute_timestamp": "parse_absolute_timestamp"},
    "hermes_cli.config": {"load_config": "load_config",
                          "clear_model_endpoint_credentials": "clear_model_endpoint_credentials"},
    "hermes_cli.route_identity": {"normalize_route_base_url": "normalize_route_base_url"},
    "hermes_cli.model_normalize": {"normalize_model_for_provider": "normalize_model_for_provider"},
    "hermes_cli.providers": {"get_provider": "get_provider",
                             "normalize_provider": "normalize_provider",
                             "resolve_custom_provider": "resolve_custom_provider",
                             "resolve_user_provider": "resolve_user_provider"},
    "utils": {"base_url_hostname": "base_url_hostname"},
    "agent.model_metadata": {"_infer_provider_from_url": "infer_provider_from_url"},
    "agent.retry_utils": {"parse_retry_after_seconds": "parse_retry_after_seconds",
                          "reset_delay_from_message": "reset_delay_from_message"},
    "agent.error_classifier": {"CODEX_ACCOUNT_MODEL_ENTITLEMENT_MARKER": None},
}

CURRENT = {"rec": None}
_MISSING = object()


def _stub_fn(seam):
    def f(*a, **k):
        return CURRENT["rec"](seam)(*a, **k)
    return f


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


def build_exc(spec):
    kind = spec.get("__raise__", "RuntimeError")
    msg = spec.get("message", "")
    base = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
    builtin = {"TypeError": TypeError, "ValueError": ValueError,
               "RuntimeError": RuntimeError, "AttributeError": AttributeError,
               "KeyError": KeyError, "TimeoutError": TimeoutError,
               "Exception": Exception, "OSError": OSError,
               "StopIteration": StopIteration, "ArithmeticError": ArithmeticError,
               "LookupError": LookupError, "ImportError": ImportError,
               "NotImplementedError": NotImplementedError}
    if kind in base:
        cls = base[kind]
    elif kind in builtin:
        cls = builtin[kind]
    elif kind == "CancelledError":
        import asyncio
        cls = asyncio.CancelledError
    else:
        cls = type(kind, (Exception,), {})
    return cls(msg)


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
    if isinstance(v, SimpleNamespace):
        return {k: norm_value(x) for k, x in vars(v).items()}
    if isinstance(v, BaseException):
        return str(v)
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
    if isinstance(v, enum.Enum):
        return v.value
    return v


def norm_args(v):
    v = norm_value(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_args(x) for x in v]
    return v


def proj(name, a, k):
    if name == "parse_absolute_timestamp": return {"reset_at": a[0]}
    if name in ("time.time", "time.monotonic", "load_config"): return {}
    if name == "normalize_route_base_url": return {"base_url": a[0]}
    if name in ("logging.info", "logger.warning"): return {"fmt": a[0], "args": list(a[1:])}
    if name == "pool.has_available": return {"model": k.get("model")}
    if name == "buffer_diagnostic_status": return {"text": a[0]}
    if name == "normalize_model_for_provider": return {"model": a[0], "provider": a[1]}
    if name == "base_url_hostname": return {"url": a[0]}
    if name == "normalize_provider": return {"provider": a[0]}
    if name == "resolve_user_provider": return {"provider": a[0], "providers": a[1]}
    if name == "resolve_custom_provider": return {"provider": a[0], "custom_providers": a[1]}
    if name == "get_provider": return {"provider": a[0], "allow_network": k.get("allow_network")}
    if name == "infer_provider_from_url": return {"base_url": a[0]}
    if name == "clear_model_endpoint_credentials":
        # the model_cfg carrier travels via st, not seam args
        return {"clear_api_mode": k.get("clear_api_mode", True)}
    if name == "parse_retry_after_seconds": return {"value": a[0]}
    if name == "reset_delay_from_message": return {"message": a[0]}
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
            if name == "clear_model_endpoint_credentials":
                # identical live effect to the C port (call site passes
                # clear_api_mode=False; clear_api_key defaults True,
                # clear_base_url defaults False)
                mc = a[0] if a else None
                if isinstance(mc, dict):
                    if k.get("clear_api_key", True):
                        for key in ("api_key", "api", "key_env", "api_key_env"):
                            mc.pop(key, None)
                    if k.get("clear_api_mode", True):
                        mc.pop("api_mode", None)
                    if k.get("clear_base_url", False):
                        mc.pop("base_url", None)
                return mc
            return materialize(name, raw)
        return call


def materialize(name, raw):
    if name in ("resolve_user_provider", "resolve_custom_provider", "get_provider"):
        if isinstance(raw, dict):
            return SimpleNamespace(**raw)
        return raw
    return raw


class RecLogger:
    def _log(self, lvl):
        def f(fmt, *args):
            return CURRENT["rec"]("logger." + lvl)(fmt, *args)
        return f

    def __getattr__(self, name):
        if name in ("info", "warning", "debug", "error"):
            return self._log(name)
        raise AttributeError(name)


class ApiErrorDouble:
    """api_error object double: present-or-absent status_code/message attrs and
    an explicit str() projection (the "str" fixture field)."""
    def __init__(self, d):
        if "status_code" in d:
            self.status_code = d["status_code"]
        if "message" in d:
            self.message = d["message"]
        self._str = d.get("str", "")

    def __str__(self):
        return self._str


def make_pool(raw):
    if not isinstance(raw, dict):
        return raw
    p = SimpleNamespace()
    p.has_available = CURRENT["rec"]("pool.has_available")
    return p


def make_agent(inp):
    ad = inp.get("agent") or {}
    attrs = {}
    for k, v in ad.items():
        if k == "_credential_pool":
            attrs[k] = make_pool(v)
        elif k == "_entitlement_rejected_models":
            attrs[k] = set(tuple(p) for p in v) if isinstance(v, list) else v
        else:
            attrs[k] = v
    agent = SimpleNamespace(**attrs)
    agent._buffer_diagnostic_status = CURRENT["rec"]("buffer_diagnostic_status")
    return agent


def reason_obj(inp):
    r = inp.get("reason")
    if r is None:
        return None
    return NS["FailoverReason"][r]


# ---- live namespace seeding ----
NS = {}
REAL = {}


def _exec_seg(src, node, ns, filename):
    seg = ast.get_source_segment(src, node)
    seg = textwrap.dedent(seg)
    code = compile(seg, filename, "exec", flags=ANN_FLAG, dont_inherit=True)
    exec(code, ns)


def load_ns():
    global NS, REAL
    mod = types.ModuleType("fc_live")
    sys.modules["fc_live"] = mod
    ns = mod.__dict__
    NS = ns
    ns.update({
        "__name__": "fc_live", "enum": enum, "math": math, "json": json,
        "time": SimpleNamespace(time=_stub_fn("time.time"),
                                monotonic=_stub_fn("time.monotonic")),
        "logging": SimpleNamespace(info=_stub_fn("logging.info"),
                                   getLogger=lambda name: RecLogger()),
        "logger": RecLogger(),
        "Any": object, "Optional": object,
    })
    fc_src = Path(FC_SRC).read_text(encoding="utf-8")
    ri_src = Path(RI_SRC).read_text(encoding="utf-8")
    ec_src = Path(EC_SRC).read_text(encoding="utf-8")
    fc_tree = ast.parse(fc_src)
    ri_tree = ast.parse(ri_src)
    ec_tree = ast.parse(ec_src)
    # FailoverReason enum FIRST (the rate-limit reason set constructs it)
    for node in ec_tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "FailoverReason":
            _exec_seg(ec_src, node, ns, EC_SRC)
    if "FailoverReason" not in ns:
        raise RuntimeError("seed missing: FailoverReason")
    # module constants
    for node in ec_tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if isinstance(tgt, ast.Name) and tgt.id in (
                    "_RESET_FIELDS", "_RESET_HEADERS", "_USAGE_LIMIT_TRANSIENT_SIGNALS"):
                _exec_seg(ec_src, node, ns, EC_SRC)
    for node in fc_tree.body:
        if isinstance(node, ast.Assign):
            names = {t.id for t in node.targets if isinstance(t, ast.Name)}
            if "_RATE_LIMIT_FAILOVER_REASONS" in names:
                _exec_seg(fc_src, node, ns, FC_SRC)
    # target defs
    found = set()
    for node in fc_tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in FUNCS:
            _exec_seg(fc_src, node, ns, FC_SRC)
            found.add(node.name)
            if node.name == "_on_primary_route":
                for sub in node.body:
                    if isinstance(sub, ast.FunctionDef) and sub.name == "ident":
                        _exec_seg(fc_src, sub, ns, FC_SRC)
                        found.add("ident")
    for node in ri_tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in FUNCS:
            _exec_seg(ri_src, node, ns, RI_SRC)
            found.add(node.name)
    for node in ec_tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in FUNCS:
            _exec_seg(ec_src, node, ns, EC_SRC)
            found.add(node.name)
    missing = set(FUNCS) - found
    if missing:
        raise RuntimeError("missing target defs: %s" % sorted(missing))
    # claimed module-global sibling seamed
    for live, seam in SEAM_GLOBALS.items():
        ns[live] = _stub_fn(seam)
    # in-function import stubs. OVERWRITE the parent packages (not setdefault):
    # the quarry is importable in the venv, so a stale real package would let
    # live submodule imports leak past the stubs.
    agent_pkg = types.ModuleType("agent"); agent_pkg.__path__ = []
    sys.modules["agent"] = agent_pkg
    hc_pkg = types.ModuleType("hermes_cli"); hc_pkg.__path__ = []
    sys.modules["hermes_cli"] = hc_pkg
    for modname, attrs in SEAM_IMPORTS.items():
        m = types.ModuleType(modname)
        for attr, seam in attrs.items():
            if seam is None:
                continue
            setattr(m, attr, _stub_fn(seam))
        sys.modules[modname] = m
        parts = modname.split(".")
        if len(parts) == 2:
            setattr(sys.modules[parts[0]], parts[1], m)
    # real constant into its stub module
    sys.modules["agent.error_classifier"].CODEX_ACCOUNT_MODEL_ENTITLEMENT_MARKER = \
        "model is not supported when using codex with a chatgpt account"
    # audit: every name the drivers read is bound
    for nm in FUNCS + ["FailoverReason", "_RATE_LIMIT_FAILOVER_REASONS",
                       "_RESET_FIELDS", "_RESET_HEADERS", "_USAGE_LIMIT_TRANSIENT_SIGNALS"]:
        if nm not in ns:
            raise RuntimeError("seed missing: " + nm)
    REAL = {nm: ns[nm] for nm in FUNCS}


def drive(func, inp):
    """Return (ret, st). Raises on live exception (caught by run_case)."""
    if func in ("_provider_reset_delay",):
        return NS[func](inp.get("reset_at")), {}
    if func == "ident":
        return NS[func](inp.get("provider"), inp.get("model"), inp.get("base_url")), {}
    if func in ("_error_obj", "_json_dict", "_usage_window_display_message"):
        key = "text" if func == "_json_dict" else "body"
        return NS[func](inp.get(key)), {}
    if func in ("_has_usage_limit_transient_signal", "_rate_limit_reset_seconds"):
        return NS[func](inp.get("error_msg"), inp.get("body"),
                        inp.get("response_headers")), {}
    if func == "provider_owns_route":
        return NS[func](inp.get("provider"), inp.get("base_url"), inp.get("config")), {}
    if func == "drop_stale_model_route":
        mc = copy.deepcopy(inp.get("model_cfg"))
        ret = NS[func](mc, inp.get("provider"), inp.get("config"))
        return ret, {"model_cfg": mc}
    if func == "drop_credentials_of_other_route":
        mc = copy.deepcopy(inp.get("model_cfg"))
        ret = NS[func](mc, inp.get("prior_model_cfg"))
        return ret, {"model_cfg": mc}
    agent = make_agent(inp)
    if func == "switch_deferred_by_reset":
        return NS[func](agent, reason_obj(inp), inp.get("reset_at")), {}
    if func == "_on_primary_route":
        return NS[func](agent), {}
    if func == "_arm_rate_limit_cooldown":
        ret = NS[func](agent, reason_obj(inp), inp.get("reset_at"))
        st = {"_rate_limit_backoff_count": getattr(agent, "_rate_limit_backoff_count", 0),
              "_rate_limited_until": getattr(agent, "_rate_limited_until", None)}
        return ret, st
    if func == "_mark_entitlement_rejected_model":
        ed = inp.get("api_error")
        err = ApiErrorDouble(ed) if isinstance(ed, dict) else ed
        ret = NS[func](agent, err)
        rej = getattr(agent, "_entitlement_rejected_models", None)
        st = {}
        if rej is not None:
            st["_entitlement_rejected_models"] = sorted([list(t) for t in rej])
        return ret, st
    if func == "_is_entitlement_rejected":
        return NS[func](agent, inp.get("provider"), inp.get("model")), {}
    raise RuntimeError("unknown func " + func)


def run_case(func, inp):
    for nm, fn in REAL.items():
        NS[nm] = fn
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
        got = {"ret": rec.get("ret"), "trace": rec.get("trace"), "st": rec.get("st")}
        got = norm_value(got)
        if got != want:
            mism += 1
            if mism <= 12:
                print("MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:600]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:600]))
    print("ORACLE fallback_cooldown_flow: %d cases, %d mismatches" % (n, mism))
    if n == 0:
        sys.exit(1)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
