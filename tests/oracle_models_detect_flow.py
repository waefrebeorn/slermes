#!/usr/bin/env python3
"""Oracle for the hermes_cli/models_detect.py FLOW port
(src/cli/models_detect_flow.c).

Drives the LIVE models_detect.py defs with SCRIPTED seams: every catalog/auth/
config/credential-pool dependency is a recording stub fed the same scripts the C
harness feeds mdet_call. Compares the DECISION SEQUENCE (ordered "trace" of
{ev,args,ret} seam events) and the canonical return ("out"). Pure leaves
(normalize_provider, detect_vendor, has_usable_secret, _is_placeholder_shape)
stay REAL via AST extraction so nested decision logic is exercised, matching the
C port's internal transcriptions. The ported module's own _SKIP and auth's
placeholder constants are exec'd from their real Assign nodes.

Live defs are AST-extracted from the quarry (annotations kept as strings via the
__future__ flag). In-function imports resolve through per-case sys.modules stubs
(hermes_cli.models / model_normalize / auth / config, agent.credential_pool);
import-failure gating sets sys.modules[full]=None. The heavy quarry import chain
is never imported. Fail-closed: 0 cases => exit 1; any harness/oracle exception
per case rides as {"__raise__": cls}. BaseException-only kinds (SystemExit,
KeyboardInterrupt, GeneratorExit, ...) propagate through the live `except
Exception` sites; synthetic unknown raise names are Exception subclasses.
Summary: 'ORACLE models_detect_flow: N cases, M mismatches'.

Representation contract (both sides, see src/cli/models_detect_flow.c header):
- None -> null; bool -> bool; uncaught exception -> {"__raise__": cls}.
- scripts: absent key -> None; {"$seq": [...]} consumed per call (exhaustion
  raises RuntimeError("script exhausted: <name>")); {"__raise__": cls, "msg"}
  raises; any other value reused. None-valued arg keys dropped on both sides.
- str()/strip()/lower() operand fields are ASCII strings/null in fixtures.
- env tables: aliases, vendor_prefixes (ordered), aggregators, provider_models,
  import_fail. load_pool script is null or an object double ({"pool": true}).
"""
import __future__
import ast
import builtins
import json
import logging
import sys
import types
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stdin, "reconfigure"):
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "models_detect.py").is_file():
    QUARRY = Path.home() / "hermes-agent"

NAME = "models_detect_flow"

PORTED = [
    "current_provider_catalog_match",
    "current_provider_owns_vendor",
    "provider_has_credentials",
]
PURE = ["normalize_provider", "detect_vendor", "has_usable_secret", "_is_placeholder_shape"]
WANTED = set(PORTED) | set(PURE)

# (source file, kind, name) — kind: "def" or "const"
EXTRACT = [
    ("hermes_cli/models_detect.py", "def", "current_provider_catalog_match"),
    ("hermes_cli/models_detect.py", "def", "current_provider_owns_vendor"),
    ("hermes_cli/models_detect.py", "def", "provider_has_credentials"),
    ("hermes_cli/models_detect.py", "const", "_SKIP"),
    ("hermes_cli/models.py", "def", "normalize_provider"),
    ("hermes_cli/model_normalize.py", "def", "detect_vendor"),
    ("hermes_cli/auth.py", "def", "has_usable_secret"),
    ("hermes_cli/auth.py", "def", "_is_placeholder_shape"),
    ("hermes_cli/auth.py", "const", "_PLACEHOLDER_SECRET_VALUES"),
    ("hermes_cli/auth.py", "const", "_PLACEHOLDER_KEY_PREFIXES"),
]

logging.disable(logging.CRITICAL)

# ── AST extraction ──────────────────────────────────────────────────────────

_FLAGS = __future__.annotations.compiler_flag
_TREES = {}


def _tree(rel):
    if rel not in _TREES:
        _TREES[rel] = ast.parse((QUARRY / rel).read_text(encoding="utf-8"))
    return _TREES[rel]


DEF_CODE = {}
CONST_CODE = {}
_missing = []
for _rel, _kind, _nm in EXTRACT:
    _src = str(QUARRY / _rel)
    _node = None
    for _n in _tree(_rel).body:
        if _kind == "def" and isinstance(_n, ast.FunctionDef) and _n.name == _nm:
            _node = _n
            break
        if _kind == "const" and isinstance(_n, ast.Assign):
            if any(isinstance(t, ast.Name) and t.id == _nm for t in _n.targets):
                _node = _n
                break
    if _node is None:
        _missing.append(f"{_rel}:{_nm}")
        continue
    _code = compile(ast.Module(body=[_node], type_ignores=[]), _src, "exec",
                    flags=_FLAGS, dont_inherit=True)
    if _kind == "def":
        DEF_CODE[_nm] = _code
    else:
        CONST_CODE[_nm] = _code
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {_missing}", file=sys.stderr)
    sys.exit(1)
if WANTED - set(DEF_CODE):
    print(f"ORACLE {NAME}: FATAL missing funcs: {sorted(WANTED - set(DEF_CODE))}", file=sys.stderr)
    sys.exit(1)

_BASE_NS = {
    "__name__": "mdet_live_models_detect",
    "__builtins__": builtins,
}
# py3.14 annotation resolution wants a registered module.
_MOD = types.ModuleType("mdet_live_models_detect")
_MOD.__dict__.update(_BASE_NS)
sys.modules.setdefault("mdet_live_models_detect", _MOD)

# ── canonical representation ────────────────────────────────────────────────


def norm(v):
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def denorm(v):
    if isinstance(v, dict):
        return {k: denorm(x) for k, x in v.items()}
    if isinstance(v, list):
        return [denorm(x) for x in v]
    return v


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    # ExceptionGroup/BaseExceptionGroup require (message, exceptions); cls(msg)
    # alone raises TypeError, which would mask the scripted kind.
    if issubclass(cls, BaseExceptionGroup):
        if issubclass(cls, ExceptionGroup):
            return cls(msg, [Exception("inner")])
        # BaseExceptionGroup auto-converts to ExceptionGroup when all contents
        # are Exception; a non-Exception inner keeps it a true BaseExceptionGroup.
        return cls(msg, [KeyboardInterrupt("inner")])
    return cls(msg)


# ── scripted seams ──────────────────────────────────────────────────────────

ARGMAP = {
    "cached_provider_model_ids": lambda a, k: {"provider": a[0]},
    "get_env_value_prefer_dotenv": lambda a, k: {"key": a[0]},
    "get_auth_status": lambda a, k: {"provider_id": a[0]},
    "load_pool": lambda a, k: {"provider": a[0]},
    "has_credentials": lambda a, k: {},
    "has_available": lambda a, k: {},
}


class Scripts:
    def __init__(self, io, trace):
        self.io = io or {}
        self.trace = trace
        self.cursors = {}

    def next(self, name):
        script = self.io.get(name)
        if isinstance(script, dict) and "$seq" in script:
            seq = script["$seq"]
            i = self.cursors.get(name, 0)
            if i >= len(seq):
                return {"__raise__": "RuntimeError", "msg": f"script exhausted: {name}"}
            self.cursors[name] = i + 1
            return seq[i]
        return script


def make_pool(d, scripts):
    if not isinstance(d, dict):
        return d  # null -> None (live None.has_credentials -> AttributeError)
    p = types.SimpleNamespace(**d)
    p.has_credentials = types.MethodType(lambda self: recorder("has_credentials", scripts)(), p)
    p.has_available = types.MethodType(lambda self: recorder("has_available", scripts)(), p)
    return p


def convert(name, value, scripts):
    if name == "load_pool":
        return make_pool(value, scripts)
    return value


def recorder(name, scripts):
    def rec(*a, **kw):
        args = {k: v for k, v in ARGMAP[name](a, kw).items() if v is not None}
        script = scripts.next(name)
        if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
            marker = {"__raise__": script["__raise__"], "msg": script.get("msg", "")}
            scripts.trace.append({"ev": name, "args": args, "ret": marker})
            raise make_exc(marker)
        value = convert(name, denorm(script), scripts)
        scripts.trace.append({"ev": name, "args": args, "ret": norm(value)})
        return value
    return rec


def install_module_stubs(ns, env, scripts):
    hc = types.ModuleType("hermes_cli")
    hc.__path__ = []
    models = types.ModuleType("hermes_cli.models")
    models.cached_provider_model_ids = recorder("cached_provider_model_ids", scripts)
    models.normalize_provider = ns["normalize_provider"]
    models._AGGREGATOR_PROVIDERS = frozenset(env.get("aggregators") or [])
    # Preserve values verbatim (null stays None): owns_vendor iterates the raw
    # value, so a present-null row raises TypeError at CALL time (uncaught),
    # matching the C port. Eager list(v) would raise during seeding instead.
    models._PROVIDER_MODELS = {k: (list(v) if isinstance(v, list) else v)
                               for k, v in (env.get("provider_models") or {}).items()}
    mn = types.ModuleType("hermes_cli.model_normalize")
    mn.detect_vendor = ns["detect_vendor"]
    auth = types.ModuleType("hermes_cli.auth")
    auth.get_auth_status = recorder("get_auth_status", scripts)
    auth.has_usable_secret = ns["has_usable_secret"]
    cfg = types.ModuleType("hermes_cli.config")
    cfg.get_env_value_prefer_dotenv = recorder("get_env_value_prefer_dotenv", scripts)
    agent = types.ModuleType("agent")
    agent.__path__ = []
    cp = types.ModuleType("agent.credential_pool")
    cp.load_pool = recorder("load_pool", scripts)
    hc.models, hc.model_normalize, hc.auth, hc.config = models, mn, auth, cfg
    agent.credential_pool = cp
    fail = set(env.get("import_fail") or [])
    mods = {
        "hermes_cli": hc, "hermes_cli.models": models, "hermes_cli.model_normalize": mn,
        "hermes_cli.auth": auth, "hermes_cli.config": cfg,
        "agent": agent, "agent.credential_pool": cp,
    }
    for full, mod in mods.items():
        sys.modules[full] = None if full in fail else mod


# ── per-case namespace ──────────────────────────────────────────────────────


def build_ns(rec):
    env = rec["in"].get("env") or {}
    ns = dict(_BASE_NS)
    for code in CONST_CODE.values():
        exec(code, ns)
    ns["_PROVIDER_ALIASES"] = dict(env.get("aliases") or {})
    ns["_VENDOR_PREFIXES"] = dict(env.get("vendor_prefixes") or {})
    for nm in PURE:
        exec(DEF_CODE[nm], ns)
    for nm in PORTED:
        exec(DEF_CODE[nm], ns)
    trace = []
    scripts = Scripts(rec["in"].get("io") or {}, trace)
    install_module_stubs(ns, env, scripts)
    return ns, trace, scripts


# ── live driver ─────────────────────────────────────────────────────────────


def drive(rec, ns, scripts):
    func = rec["func"]
    inp = rec["in"]
    if func in ("current_provider_catalog_match", "current_provider_owns_vendor"):
        return ns[func](inp.get("model_name"), inp.get("current_provider"))
    if func == "provider_has_credentials":
        return ns[func](inp.get("provider"))
    raise KeyError(f"unknown func {func}")


def run_case(rec):
    """Build + drive one case; returns (out, trace). Fail-closed per case."""
    try:
        ns, trace, scripts = build_ns(rec)
        try:
            out = norm(drive(rec, ns, scripts))
        except BaseException as e:  # noqa: BLE001 - fail-closed per case
            out = {"__raise__": type(e).__name__}
    except BaseException as e:  # noqa: BLE001 - seeding failure is a mismatch
        out = {"__raise__": type(e).__name__}
        trace = []
    return out, trace


def main():
    n = mism = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        out, trace = run_case(rec)
        got_out = rec.get("out")
        got_trace = rec.get("trace")
        if got_out != out or got_trace != trace:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                if got_out != out:
                    print(f"  out  C={json.dumps(got_out, ensure_ascii=False)[:400]}")
                    print(f"  out  P={json.dumps(out, ensure_ascii=False)[:400]}")
                if got_trace != trace:
                    print(f"  trace C={json.dumps(got_trace, ensure_ascii=False)[:600]}")
                    print(f"  trace P={json.dumps(trace, ensure_ascii=False)[:600]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()