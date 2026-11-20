#!/usr/bin/env python3
"""Oracle for src/cli/observability_metrics_flow.c — drives the LIVE
hermes_cli/observability/shared_metrics_fields.py FLOW + leaf defs (20
functions; the task-named metrics.py does not exist — shared_metrics_fields.py
is the real defining module) under IDENTICAL scripted universal seams.

Reads the harness JSONL on stdin ({"func","in","ret","trace","st"}), recomputes
the LIVE reference from rec["in"], and compares the ordered trace, the return
(or raise spec), and the state window ({} — the field builders are pure). Live
defs are AST-extracted; the module constants (threshold tables, _USAGE_KEYS,
compression tables, _NETWORK_MODULES, _MILESTONE_RULES) are seeded, and the
contract closed-enum constants the builders read are AST-extracted from the
LIVE shared_metrics_contract.py (pure literal Assigns, source order). All
cross-module collaborators (contract._norm / _bucket / count_bucket /
size_bucket / execution_surface / model_error_class / _non_negative_number,
catalog.provider_metric_name / model_metric_name / aux_task_metric_name /
slash_command_metric_name / extension_metric_name, signals.user_created_skill)
are recording seams fed the SAME scripts the C harness feeds. Ported siblings
stay REAL so composition is exercised (install_age_bucket -> _number,
compression_fields -> context_fill_bucket + compression_failure_class,
model_token_fields -> provider_identifier twice, extension_install_fields ->
extension_failure_class + extension_registry, extension_failure_class ->
exception_failure_class -> tagged_failure_class, milestones_for ->
_long_session).

Fail-closed: an absent / exhausted script RETURNS a RuntimeError raise spec
("no script for <name>"), recorded as the event ret before propagation; 0 cases
=> exit 1. A scripted BaseException (SystemExit / KeyboardInterrupt /
GeneratorExit) propagates past everything exactly as live (the module has no
except guards). Per-case BaseException catch -> __raise__ spec; scripted raises
round-trip BARE via identity match against Script.handed. Real raises
(None.get AttributeError, py3.14 unhashable-set TypeError, unknown-kind
KeyError, None.split AttributeError) carry the LIVE type name + message.

Value model: exception doubles {"__exc__": true, "type", "module", "mro",
"failure_class"?, "message"?, "hook"?, "failure_class_property"?} materialize
to REAL exception instances (builtin classes from the full 3.14 hierarchy;
socket.gaierror / urllib.error.URLError real; synthetic classes built from the
scripted mro, optional raising __getattr__ / property to prove getattr_static
bypass). Plain object doubles are SimpleNamespace (getattr .value / static
failure_class). tuple/set -> array; integral floats -> ints.

Domain (ASCII scope, documented in the port header): _norm seam results are
str | null | raise; _non_negative_number results number | null | raise; usage
amounts are INTEGER literals or non-integral floats / bools / strs / nulls
(integral floats excluded — libjson has no int/float identity); floats in seam
args/rets are integral or binary-exact short decimals; error inputs are exc
doubles, attribute doubles, null or str.

Summary: 'ORACLE observability_metrics_flow: N cases, M mismatches'.
"""
import __future__
import ast
import builtins
import copy
import json
import sys
import types
from pathlib import Path
from types import SimpleNamespace
from typing import Any

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "hermes_cli" / "observability" / "shared_metrics_fields.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "observability" / "shared_metrics_fields.py")
CONTRACT_SRC = str(QUARRY / "hermes_cli" / "observability" / "shared_metrics_contract.py")
HUB_SRC = str(QUARRY / "tools" / "skills_hub_install.py")
NAME = "observability_metrics_flow"

PORTED = [
    "_number", "provider_identifier", "session_fields", "install_age_bucket",
    "ttft_bucket", "context_fill_bucket", "model_token_fields",
    "setup_completed_fields", "compression_failure_class", "compression_fields",
    "model_switch_fields", "fallback_fields", "slash_command_fields",
    "extension_install_fields", "tagged_failure_class", "exception_failure_class",
    "extension_failure_class", "extension_registry", "_long_session",
    "milestones_for",
]
RULES_CONST = "_MILESTONE_RULES"
CONTRACT_CONSTANTS = [
    "MODEL_IDENTIFIER_MAX_LENGTH", "PROVIDER_IDENTIFIER_MAX_LENGTH",
    "TASK_OUTCOMES", "SETUP_SURFACES",
    "COMPRESSION_SKIP_CLASSES", "COMPRESSION_FAILURE_CLASSES",
    "EXTENSION_KINDS", "EXTENSION_SOURCES",
    "EXTENSION_COMMON_FAILURE_CLASSES", "EXTENSION_SKILL_FAILURE_CLASSES",
    "EXTENSION_PLUGIN_FAILURE_CLASSES", "EXTENSION_MCP_FAILURE_CLASSES",
    "EXTENSION_KIND_FAILURE_CLASSES", "EXTENSION_REGISTRY_IDS", "EXTENSION_REGISTRIES",
    "TASK_STARTED_METRIC", "TASK_FINISHED_METRIC", "TOOL_USAGE_METRIC",
    "SKILL_LIFECYCLE_METRIC", "SKILL_LOAD_METRIC", "SESSION_METRIC",
    "SETUP_COMPLETED_METRIC",
]

# ── AST extraction ──────────────────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node):
    return compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
CONST_CODE = {}
RULES_CODE = None
for _node in _TREE.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in PORTED:
        DEF_CODE[_node.name] = _compile_node(_node)
    elif isinstance(_node, (ast.Assign, ast.AnnAssign)):
        _tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(_tg, ast.Name):
            if _tg.id == RULES_CONST:
                RULES_CODE = _compile_node(_node)
            else:
                CONST_CODE[_tg.id] = _compile_node(_node)
_missing = set(PORTED) - set(DEF_CODE)
if _missing or RULES_CODE is None:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)} rules={RULES_CODE is None}",
          file=sys.stderr)
    sys.exit(1)

# contract closed enums: pure literal Assigns, exec in SOURCE order so the
# union dependencies (COMPRESSION_SKIP_CLASSES, EXTENSION_COMMON_*,
# EXTENSION_REGISTRY_IDS) bind before their users. Fail-closed on any gap.
_CTR_TEXT = Path(CONTRACT_SRC).read_text(encoding="utf-8")
_CTR_TREE = ast.parse(_CTR_TEXT)
CTR = {"__builtins__": builtins}
_ctr_bound = set()
for _node in _CTR_TREE.body:
    if isinstance(_node, (ast.Assign, ast.AnnAssign)):
        _tg = _node.targets[0] if isinstance(_node, ast.Assign) else _node.target
        if isinstance(_tg, ast.Name) and _tg.id in CONTRACT_CONSTANTS:
            exec(compile(ast.Module(body=[_node], type_ignores=[]), CONTRACT_SRC, "exec",
                         flags=_FLAGS, dont_inherit=True), CTR)
            _ctr_bound.add(_tg.id)
_ctr_missing = set(CONTRACT_CONSTANTS) - _ctr_bound
if _ctr_missing:
    print(f"ORACLE {NAME}: FATAL unseeded contract constants: {sorted(_ctr_missing)}",
          file=sys.stderr)
    sys.exit(1)

# tools.skills_hub_install._SOURCE_ID_ALIASES (in-function import constant).
_HUB_TEXT = Path(HUB_SRC).read_text(encoding="utf-8")
_HUB_TREE = ast.parse(_HUB_TEXT)
SOURCE_ID_ALIASES = None
for _node in _HUB_TREE.body:
    if isinstance(_node, ast.Assign):
        for _tg in _node.targets:
            if isinstance(_tg, ast.Name) and _tg.id == "_SOURCE_ID_ALIASES":
                _ns = {"__builtins__": builtins}
                exec(compile(ast.Module(body=[_node], type_ignores=[]), HUB_SRC, "exec",
                             flags=_FLAGS, dont_inherit=True), _ns)
                SOURCE_ID_ALIASES = _ns["_SOURCE_ID_ALIASES"]
if not isinstance(SOURCE_ID_ALIASES, dict):
    print(f"ORACLE {NAME}: FATAL cannot seed _SOURCE_ID_ALIASES", file=sys.stderr)
    sys.exit(1)

# ── canonical projection ────────────────────────────────────────────────────


def norm_value(v):
    if isinstance(v, BaseException):
        return str(v)
    if isinstance(v, SimpleNamespace):
        return {k: norm_value(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, dict):
        return {k: norm_value(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [norm_value(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm_value(x) for x in v), key=lambda s: str(s))
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def norm_args(v):
    v = norm_value(v)
    if isinstance(v, dict):
        return {k: norm_args(x) for k, x in v.items() if x is not None}
    if isinstance(v, list):
        return [norm_args(x) for x in v]
    return v


# full builtin exception hierarchy (CPython 3.14) for scripted-raise + double
# materialization; the trio are the REAL builtins so they propagate.
BUILTIN_EXC = {}
for _nm in [
    "BaseException", "SystemExit", "KeyboardInterrupt", "GeneratorExit",
    "Exception", "StopIteration", "StopAsyncIteration", "ArithmeticError",
    "FloatingPointError", "OverflowError", "ZeroDivisionError", "AssertionError",
    "AttributeError", "BufferError", "EOFError", "ImportError",
    "ModuleNotFoundError", "LookupError", "IndexError", "KeyError", "MemoryError",
    "NameError", "UnboundLocalError", "OSError", "BlockingIOError",
    "ChildProcessError", "ConnectionError", "BrokenPipeError",
    "ConnectionAbortedError", "ConnectionRefusedError", "ConnectionResetError",
    "FileExistsError", "FileNotFoundError", "InterruptedError", "IsADirectoryError",
    "NotADirectoryError", "PermissionError", "ProcessLookupError", "TimeoutError",
    "ReferenceError", "RuntimeError", "NotImplementedError", "RecursionError",
    "PythonFinalizationError", "SyntaxError", "IndentationError", "TabError",
    "_IncompleteInputError", "SystemError", "TypeError", "ValueError",
    "UnicodeError", "UnicodeDecodeError", "UnicodeEncodeError",
    "UnicodeTranslateError", "Warning", "DeprecationWarning",
    "PendingDeprecationWarning", "RuntimeWarning", "SyntaxWarning", "UserWarning",
    "FutureWarning", "ImportWarning", "UnicodeWarning", "BytesWarning",
    "EncodingWarning", "ResourceWarning", "BaseExceptionGroup", "ExceptionGroup",
]:
    _cls = getattr(builtins, _nm, None)
    if isinstance(_cls, type) and issubclass(_cls, BaseException):
        BUILTIN_EXC[_nm] = _cls


def make_exc(marker):
    kind = marker.get("__raise__", "RuntimeError")
    msg = marker.get("message", marker.get("msg", ""))
    cls = BUILTIN_EXC.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    return cls(msg)


# ── exception / attribute doubles ───────────────────────────────────────────


def _raise_hook(self, name):
    raise RuntimeError("hook fired")


def _raise_prop(self):
    raise RuntimeError("prop fired")


def mk_exc_double(j):
    """Materialize an exc double to a REAL exception instance."""
    t = j.get("type", "Exception")
    mod = j.get("module", "builtins")
    cls = None
    if t in BUILTIN_EXC and mod == "builtins":
        cls = BUILTIN_EXC[t]
    elif t == "gaierror" and mod == "socket":
        import socket
        cls = socket.gaierror
    elif t == "URLError" and mod == "urllib.error":
        import urllib.error
        cls = urllib.error.URLError
    else:
        mro = j.get("mro") or []
        base = Exception
        for e in reversed(mro):
            nm = e.get("name")
            md = e.get("module", "builtins")
            if nm == "object":
                continue
            if nm in BUILTIN_EXC and md == "builtins":
                base = BUILTIN_EXC[nm]
                continue
            attrs = {"__module__": md}
            if nm == t:
                attrs["__module__"] = mod
                if j.get("hook"):
                    attrs["__getattr__"] = _raise_hook
                if j.get("failure_class_property"):
                    attrs["failure_class"] = property(_raise_prop)
            cls = type(nm, (base,), attrs)
            base = cls
        if cls is None:
            cls = type(t, (Exception,), {"__module__": mod})
    if issubclass(cls, BaseExceptionGroup):
        # BaseExceptionGroup.__new__ requires (message, exceptions); the str
        # form is never read by the classifiers (type/mro/tag only).
        e = cls(j.get("message", ""), [ValueError("inner")])
    else:
        e = cls.__new__(cls)
        Exception.__init__(e, j.get("message", ""))
    if "failure_class" in j:
        e.__dict__["failure_class"] = j["failure_class"]
    return e


def mk_error(j):
    if isinstance(j, dict):
        if j.get("__exc__"):
            return mk_exc_double(j)
        return SimpleNamespace(**j)
    return j


# ── scripted seams (fail-closed) ────────────────────────────────────────────
_MISSING = object()
CURRENT = {"ctx": None}


class Raise:
    def __init__(self, spec, exc):
        self.spec = spec
        self.exc = exc


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
        return self._unwrap(val)

    def _fail(self, name):
        spec = {"__raise__": "RuntimeError", "message": "no script for " + name}
        r = Raise(spec, make_exc(spec))
        self.handed.append(r)
        return r

    def _unwrap(self, val):
        if isinstance(val, dict) and "__raise__" in val:
            r = Raise(val, make_exc(val))
            self.handed.append(r)
            return r
        if isinstance(val, dict) and val.get("__script__") is True:
            return val.get("ret")
        return val


def proj(name, a, k):
    if name in ("_norm", "contract._non_negative_number"):
        return {"value": a[0] if a else None}
    if name == "_bucket":
        return {"value": a[0], "thresholds": a[1], "last": a[2]}
    if name in ("contract.count_bucket", "contract.size_bucket"):
        return {"count": a[0]}
    if name in ("contract.execution_surface", "contract.model_error_class"):
        return {"kwargs": a[0]}
    if name in ("catalog.provider_metric_name", "catalog.aux_task_metric_name",
                "catalog.slash_command_metric_name"):
        return {"raw": a[0] if a else None}
    if name == "catalog.model_metric_name":
        return {"raw": a[0], "provider": a[1], "max_length": k.get("max_length")}
    if name == "catalog.extension_metric_name":
        return {"kind": a[0], "raw": a[1]}
    if name == "user_created_skill":
        return {"d": a[0]}
    return {**{("arg%d" % i): norm_value(v) for i, v in enumerate(a)},
            **{kk: norm_value(vv) for kk, vv in k.items()}}


def record(name, argsdict):
    ctx = CURRENT["ctx"]
    args = norm_args(argsdict)
    raw = ctx.script.next(name)
    if isinstance(raw, Raise):
        ctx.trace.append({"ev": name, "args": args, "ret": raw.spec})
        raise raw.exc
    ctx.trace.append({"ev": name, "args": args, "ret": norm_value(raw)})
    return raw


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **k):
        return record(self.name, proj(self.name, a, k))


# ── live namespace ──────────────────────────────────────────────────────────
_MOD = types.ModuleType("om_live")
sys.modules["om_live"] = _MOD
NS = _MOD.__dict__
NS.update({
    "__name__": "om_live", "__builtins__": builtins, "Any": Any,
    "contract": SimpleNamespace(
        _non_negative_number=SeamFn("contract._non_negative_number"),
        count_bucket=SeamFn("contract.count_bucket"),
        size_bucket=SeamFn("contract.size_bucket"),
        execution_surface=SeamFn("contract.execution_surface"),
        model_error_class=SeamFn("contract.model_error_class"),
        **{k: CTR[k] for k in CONTRACT_CONSTANTS},
    ),
    "catalog": SimpleNamespace(
        provider_metric_name=SeamFn("catalog.provider_metric_name"),
        model_metric_name=SeamFn("catalog.model_metric_name"),
        aux_task_metric_name=SeamFn("catalog.aux_task_metric_name"),
        slash_command_metric_name=SeamFn("catalog.slash_command_metric_name"),
        extension_metric_name=SeamFn("catalog.extension_metric_name"),
    ),
    "_bucket": SeamFn("_bucket"),
    "_norm": SeamFn("_norm"),
    "_metric_identifier": SeamFn("_metric_identifier"),
    "user_created_skill": SeamFn("user_created_skill"),
    "MODEL_IDENTIFIER_MAX_LENGTH": CTR["MODEL_IDENTIFIER_MAX_LENGTH"],
    "PROVIDER_IDENTIFIER_MAX_LENGTH": CTR["PROVIDER_IDENTIFIER_MAX_LENGTH"],
})
# simple module constants first, then the defs, then _MILESTONE_RULES (it
# captures user_created_skill + _long_session + the contract metric names).
for _nm in CONST_CODE:
    exec(CONST_CODE[_nm], NS)
for _nm in PORTED:
    exec(DEF_CODE[_nm], NS)
exec(RULES_CODE, NS)


def _install_import_stubs():
    # keep a REAL tools parent when the generator imported the quarry first;
    # overwrite the skills_hub_install submodule with the seeded constant.
    if "tools" in sys.modules:
        tools = sys.modules["tools"]
    else:
        tools = types.ModuleType("tools")
        tools.__path__ = []
        sys.modules["tools"] = tools
    shi = types.ModuleType("tools.skills_hub_install")
    shi._SOURCE_ID_ALIASES = dict(SOURCE_ID_ALIASES)
    sys.modules["tools.skills_hub_install"] = shi
    tools.skills_hub_install = shi


_install_import_stubs()


# ── per-case context ────────────────────────────────────────────────────────
class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.script = Script(inp.get("io"))


def drive(func, inp):
    if func == "_number":
        return NS["_number"](inp.get("value"))
    if func == "provider_identifier":
        return NS["provider_identifier"](inp.get("value"))
    if func == "session_fields":
        return NS["session_fields"](
            inp.get("start_fields", {}), turns=inp["turns"],
            failed_turns=inp["failed_turns"], last_outcome=inp["last_outcome"],
            active_ms=inp["active_ms"])
    if func == "install_age_bucket":
        return NS["install_age_bucket"](inp.get("age_seconds"))
    if func == "ttft_bucket":
        return NS["ttft_bucket"](inp.get("event", {}))
    if func == "context_fill_bucket":
        return NS["context_fill_bucket"](inp.get("tokens_before"), inp.get("context_length"))
    if func == "model_token_fields":
        return NS["model_token_fields"](
            inp.get("usage"), model=inp.get("model"), provider=inp.get("provider"),
            call_role=inp["call_role"], aux_task=inp.get("aux_task"))
    if func == "setup_completed_fields":
        return NS["setup_completed_fields"](surface=inp.get("surface"), provider=inp.get("provider"))
    if func == "compression_failure_class":
        return NS["compression_failure_class"](inp["outcome"], inp.get("failure_class"))
    if func == "compression_fields":
        return NS["compression_fields"](
            trigger=inp.get("trigger"), outcome=inp.get("outcome"),
            tokens_before=inp.get("tokens_before"), context_length=inp.get("context_length"),
            failure_class=inp.get("failure_class"))
    if func == "model_switch_fields":
        return NS["model_switch_fields"](
            from_provider=inp.get("from_provider"), to_provider=inp.get("to_provider"),
            surface=inp.get("surface"))
    if func == "fallback_fields":
        return NS["fallback_fields"](
            from_provider=inp.get("from_provider"), to_provider=inp.get("to_provider"),
            reason=mk_error(inp.get("reason")))
    if func == "slash_command_fields":
        return NS["slash_command_fields"](command=inp.get("command"), surface=inp.get("surface"))
    if func == "extension_install_fields":
        return NS["extension_install_fields"](
            kind=inp.get("kind"), source=inp.get("source"), name=inp.get("name"),
            outcome=inp.get("outcome"), failure_class=inp.get("failure_class"),
            registry=inp.get("registry"), error=mk_error(inp.get("error")))
    if func == "tagged_failure_class":
        return NS["tagged_failure_class"](mk_error(inp.get("error")))
    if func == "exception_failure_class":
        return NS["exception_failure_class"](mk_error(inp.get("error")))
    if func == "extension_failure_class":
        return NS["extension_failure_class"](
            inp["kind"], inp["outcome"], inp.get("failure_class"),
            mk_error(inp.get("error")))
    if func == "extension_registry":
        return NS["extension_registry"](inp["kind"], inp.get("registry"))
    if func == "_long_session":
        return NS["_long_session"](inp.get("d", {}))
    if func == "milestones_for":
        return NS["milestones_for"](inp["metric_name"], inp.get("dimensions", {}))
    raise RuntimeError("unknown func " + func)


def run_case(func, inp):
    ctx = CaseCtx(inp)
    CURRENT["ctx"] = ctx
    try:
        ret = drive(func, inp)
        return {"ret": norm_value(ret), "trace": ctx.trace, "st": {}}
    except BaseException as e:  # noqa: BLE001 - fail-closed per case
        spec = None
        for r in ctx.script.handed:
            if e is r.exc:
                spec = r.spec
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        return {"ret": spec, "trace": ctx.trace, "st": {}}


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = rec.get("func")
        inp = rec.get("in") or {}
        n += 1
        want = run_case(func, inp)
        got = norm_value({"ret": rec.get("ret"), "trace": rec.get("trace"), "st": rec.get("st")})
        if got != want:
            mism += 1
            if mism <= 12:
                print("MISMATCH [%s] case %d" % (func, n))
                for key in ("ret", "trace", "st"):
                    if got.get(key) != want.get(key):
                        print("  %s GOT : %s" % (key, json.dumps(got.get(key), ensure_ascii=False)[:700]))
                        print("  %s WANT: %s" % (key, json.dumps(want.get(key), ensure_ascii=False)[:700]))
    print("ORACLE %s: %d cases, %d mismatches" % (NAME, n, mism))
    if n == 0:
        return 1
    return 1 if mism else 0


# ── self-test: drive every fixture, flag suspicious raises / drive failures ──
def selftest(fixtures):
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    bad = 0
    names = set(PORTED)
    funcs_seen = set()
    for i, rec in enumerate(fixtures):
        func = rec.get("func")
        funcs_seen.add(func)
        if func not in names:
            bad += 1
            print(f"SELFTEST case {i}: func {func} not in PORTED")
            continue
        try:
            out = run_case(func, rec.get("in") or {})
            spec = out["ret"]
            if isinstance(spec, dict) and spec.get("__raise__") == "AttributeError":
                msg = spec.get("message", "")
                if any(("'" + n + "'") in msg for n in names) or "SimpleNamespace" in msg:
                    bad += 1
                    print(f"SELFTEST case {i} {func}: suspicious AttributeError {msg}")
        except BaseException as e:  # noqa: BLE001
            bad += 1
            print(f"SELFTEST case {i} {func}: DRIVE FAILURE {type(e).__name__}: {e}")
    faulthandler.cancel_dump_traceback_later()
    missing = names - funcs_seen
    if missing:
        bad += 1
        print(f"SELFTEST: funcs with no fixture: {sorted(missing)}")
    counts = {}
    for rec in fixtures:
        counts[rec.get("func")] = counts.get(rec.get("func"), 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        bad += 1
        print(f"SELFTEST: funcs with <5 fixtures: {thin}")
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = _HERE.parent / "fixtures" / "observability_metrics_flow.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").splitlines() if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    sys.exit(main())