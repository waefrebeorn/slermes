#!/usr/bin/env python3
"""Oracle for src/agent/memory_manager_flow3.c — agent/memory_manager.py
remainder beyond flow2 (LIVE authority: C:/Users/eman5/hermes-agent/agent/memory_manager.py).

The 9 ported defs are AST-extracted from the quarry (no heavy
agent.memory_manager import). Collaborators other PoP sets own (_scrub,
_signature_params, _has_var_kwargs, initialize_all) plus the foreign-module
calls (re / _TOKEN_RE, hermes_home_key, hermes_home_key, provider callbacks,
logger, time.monotonic) are recording stubs fed the SAME scripted results the
C harness feeds its universal seam. Internal composition (redact_for_provider
-> nested walk/scrub; tool_call_keywords -> signature_params/has_var_kwargs;
initialize_for_agent -> initialize_all) runs REAL on both sides. Compares the
ordered decision sequence + return + mutated state window with order-sensitive
canon(). Per-case BaseException catch with the FULL builtin hierarchy.

Input: stdin JSON lines from t_port_memory_manager_flow3:
    {"func":..., "in":{...,"seams":{...}}, "out":{"ret":...,"trace":[...],"state":{...}}}
Summary: ORACLE memory_manager_flow3: N cases, M mismatches. Fail-closed (2)
on missing defs or zero cases.
"""
import ast
import builtins
import contextlib
import copy
import inspect
import json
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
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "mm3_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "agent", "memory_manager.py")
SRC_TEXT = open(SRC, encoding="utf-8").read().replace("\r", "")
ANN_FLAG = __import__("__future__").annotations.compiler_flag

ALL_FUNCS = {
    "redact_for_provider", "redact_messages", "gate_init", "gate_enter",
    "gate_leave", "manager_init", "tool_call_keywords", "drop_provider",
    "initialize_for_agent",
}

CUR = {"scripts": {}, "idx": {}, "raised": [], "now": 0.0}
NS = {}
F = {}
REG = {}


# ── exception construction (real builtin classes => except binds by identity) ──
_UNICODE_ERRS = ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError")


def build_exc(spec):
    cls = spec.get("__raise__", "Exception")
    msg = spec.get("msg", "")
    b = getattr(builtins, cls, None)
    if isinstance(b, type) and issubclass(b, BaseException):
        if cls in _UNICODE_ERRS:
            e = b.__new__(b)
            Exception.__init__(e, msg)
            return e
        return b(msg)
    return type(cls, (Exception,), {})(msg)


def seam(name, args):
    args = norm({k: v for k, v in args.items() if v is not None})
    scripts = CUR["scripts"]
    if name not in scripts:
        r = {"__raise__": "RuntimeError", "msg": "no script for seam: %s" % name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    r = json.loads(json.dumps(r))  # deep copy: live mutation cannot corrupt script/trace
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        e = build_exc(r)
        CUR["raised"].append((e, r))
        raise e
    return r


# ── normalization / canonical comparison ──
def norm(o):
    if o is None or isinstance(o, (bool, int, float, str)):
        return o
    if isinstance(o, (frozenset, set)):
        return sorted(norm(v) for v in o)
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, types.SimpleNamespace) and hasattr(o, "_spec"):
        return norm(o._spec)
    if isinstance(o, BaseException):
        return str(o)
    return str(o)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    o = _numnorm(o)
    if isinstance(o, dict):
        return ("obj", [(k, canon(v)) for k, v in o.items()])
    if isinstance(o, (list, tuple)):
        return ("arr", [canon(v) for v in o])
    if o is None:
        return ("null",)
    if isinstance(o, bool):
        return ("bool", o)
    if isinstance(o, (int, float)):
        return ("num", float(o))
    return ("str", o)


# ── doubles ──
_KIND = {
    "POSITIONAL_ONLY": inspect.Parameter.POSITIONAL_ONLY,
    "POSITIONAL_OR_KEYWORD": inspect.Parameter.POSITIONAL_OR_KEYWORD,
    "VAR_POSITIONAL": inspect.Parameter.VAR_POSITIONAL,
    "KEYWORD_ONLY": inspect.Parameter.KEYWORD_ONLY,
    "VAR_KEYWORD": inspect.Parameter.VAR_KEYWORD,
}


def mat_params(r):
    if not isinstance(r, dict):
        return r
    out = {}
    for k, v in r.items():
        if isinstance(v, dict):
            o = types.SimpleNamespace()
            o._spec = v
            o.kind = _KIND.get(v.get("kind"))
            out[k] = o
        else:
            out[k] = v
    return out


class InspectDouble:
    Parameter = inspect.Parameter

    @staticmethod
    def signature(fn):
        r = seam("signature", {"fn": fn})
        return types.SimpleNamespace(parameters=mat_params(r))


def seeded_signature_params(fn):
    try:
        return InspectDouble.signature(fn).parameters
    except (TypeError, ValueError):
        return None


def seeded_has_var_kwargs(params):
    return any(p.kind is inspect.Parameter.VAR_KEYWORD for p in params.values())


def mat_provider(spec):
    if not isinstance(spec, dict):
        return spec
    pid = spec.get("id")
    if pid in REG:
        return REG[pid]
    o = types.SimpleNamespace()
    o._spec = spec
    o._pid = pid
    o.name = spec.get("name", "")
    if "handle_tool_call" in spec:
        o.handle_tool_call = spec["handle_tool_call"]
    REG[pid] = o
    return o


class TokenReShim:
    @staticmethod
    def findall(text):
        r = seam("tokenize", {"text": text})
        if isinstance(r, dict):
            return r.get("tokens", [])
        return r or []


class PatternDouble:
    def __init__(self, tokens):
        self.tokens = tokens

    def search(self, text):
        return seam("redact_match", {"text": text, "tokens": self.tokens})

    def sub(self, repl, text):
        return seam("redact_sub", {"text": text, "tokens": self.tokens})


class ReShim:
    @staticmethod
    def escape(s):
        return s  # tokens are ASCII [A-Za-z0-9_\-./+]; identity is faithful for the seam

    @staticmethod
    def compile(expr):
        return PatternDouble(expr.split("|") if expr else [])


class InitLoggerDouble:
    """initialize_for_agent calls logger.warning(message) with one positional arg."""

    def warning(self, message):
        return seam("logger.warning", {"message": message})

    def log(self, *a, **kw):
        return seam("logger.log", {})

    def debug(self, *a, **kw):
        return seam("logger.debug", {})


class LoggerDouble:
    def log(self, level, fmt, *args, **kw):
        return seam("logger.log", {"level": level, "fmt": fmt, "args": list(args), **kw})

    def warning(self, fmt, *args, **kw):
        return seam("logger.warning", {"fmt": fmt, "args": list(args), **kw})


# ── manager self-double (attribute <-> state key) ──
_MAP = {
    "_providers": "providers",
    "_tool_to_provider": "tool_to_provider",
    "_external_prefetch_spill_config": "external_prefetch_spill_config",
    "_has_external": "has_external",
    "_external_prefetch_timeout": "external_prefetch_timeout",
    "_home_key": "home_key",
    "_sync_executor": "sync_executor",
    "_background_futures": "background_futures",
    "_shutting_down": "shutting_down",
    "_teardown_claim": "teardown_claim",
    "_shutdown_drain_state": "shutdown_drain_state",
    "_bound_scope": "bound_scope",
    "_agent_context": "agent_context",
    "_tool_call_keywords_cache": "tool_call_keywords_cache",
}


class SelfDouble:
    def __init__(self, st):
        object.__setattr__(self, "_st", st)

    def __getattr__(self, name):
        if name in _MAP:
            return self._st.get(_MAP[name])
        if name == "initialize_all":
            return self._initialize_all
        raise AttributeError(name)

    def __setattr__(self, name, val):
        if name in _MAP:
            self._st[_MAP[name]] = val
        else:
            object.__setattr__(self, name, val)

    def _initialize_all(self, **kwargs):
        r = seam("initialize_all", {"kwargs": kwargs})
        if isinstance(r, dict):
            self._st["providers"] = [mat_provider(p) for p in (r.get("providers") or [])]
            self._st["has_external"] = bool(r.get("has_external", False))
        else:
            self._st["providers"] = []
            self._st["has_external"] = False
        return None


# ── gate self-double backed by st["gate"] ──
class GateStateDouble:
    def __init__(self, st):
        self._st = st
        self.calls = {}
        self.last_ok = {}
        self.probe_at = {}
        self.skips = {}
        self.lock = contextlib.nullcontext()
        for rec in (st.get("gate") or []):
            key = tuple(rec["key"])
            self.calls[key] = {c[0]: (c[1], c[2]) for c in rec.get("calls", [])}
            if "last_ok" in rec:
                self.last_ok[key] = rec["last_ok"]
            if "probe_at" in rec:
                self.probe_at[key] = rec["probe_at"]
            if "skips" in rec:
                self.skips[key] = rec["skips"]

    def save(self):
        gate = []
        for key in self.calls.keys():
            rec = {"key": list(key),
                   "calls": [[t, s, d] for t, (s, d) in self.calls[key].items()]}
            if key in self.last_ok:
                rec["last_ok"] = self.last_ok[key]
            if key in self.probe_at:
                rec["probe_at"] = self.probe_at[key]
            if key in self.skips:
                rec["skips"] = self.skips[key]
            gate.append(rec)
        self._st["gate"] = gate


# ── extraction ──
ALIASES = {
    ("m", "_redact_for_provider"): "_redact_for_provider",
    ("m", "_redact_messages_for_egress"): "_redact_messages_for_egress",
    ("_StuckCallGate", "__init__"): "gate_init",
    ("_StuckCallGate", "enter"): "gate_enter",
    ("_StuckCallGate", "leave"): "gate_leave",
    ("MemoryManager", "__init__"): "mgr_init",
    ("MemoryManager", "_tool_call_keywords"): "tool_call_keywords",
    ("MemoryManager", "_drop_provider"): "drop_provider",
    ("MemoryManager", "initialize_for_agent"): "initialize_for_agent",
}


def extract():
    ns = {
        "__name__": "memory_manager_flow3_extracted",
        "__file__": SRC,
        "logger": InitLoggerDouble(),
        "inspect": InspectDouble(),
        "logging": __import__("logging"),
        "re": ReShim(),
        "_TOKEN_RE": TokenReShim(),
        "_scrub": lambda text, *, urls=True: seam(
            "redact_for_egress", {"text": text, "urls": urls}),
        "id": lambda obj: getattr(obj, "_pid", 0),
        "without_withheld_prompts": lambda msgs: seam(
            "without_withheld_prompts", {"messages": msgs}),
        "threading": types.SimpleNamespace(Lock=lambda: "<lock>"),
        "hermes_home_key": lambda: seam("hermes_home_key", {}),
        "time": types.SimpleNamespace(monotonic=lambda: CUR["now"]),
        "_signature_params": seeded_signature_params,
        "_has_var_kwargs": seeded_has_var_kwargs,
        "_EXTERNAL_PREFETCH_TIMEOUT_S": 8.0,
        "_MAX_STUCK_CALLS": 4, "_PROBE_INTERVAL_S": 60.0,
        "_MAX_INFLIGHT_CALLS": 16, "_BACKGROUND_CALL_STALL_S": 30.0,
        "_SYNC_DRAIN_TIMEOUT_S": 5.0,
        "Optional": object, "Callable": object, "Any": object,
        "Dict": object, "List": object,
    }
    tree = ast.parse(SRC_TEXT)
    found = {}
    for node in tree.body:
        if isinstance(node, ast.FunctionDef):
            key = ("m", node.name)
            if key in ALIASES:
                found[key] = node
        elif isinstance(node, ast.ClassDef):
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef):
                    key = (node.name, sub.name)
                    if key in ALIASES:
                        found[key] = sub
    missing = [a for k, a in ALIASES.items() if k not in found]
    if missing:
        print("ORACLE memory_manager_flow3: FATAL AST nodes missing: %s" % sorted(missing),
              file=sys.stderr)
        sys.exit(2)
    for key, alias in ALIASES.items():
        nd = copy.copy(found[key])
        nd.name = alias  # exec binds the alias (two classes both define __init__)
        nd.decorator_list = []
        nd.body = [x for x in nd.body
                   if not isinstance(x, (ast.Import, ast.ImportFrom))]
        try:
            exec(compile(ast.Module(body=[nd], type_ignores=[]), SRC, "exec",
                         flags=ANN_FLAG), ns)
        except Exception as exc:
            print("ORACLE memory_manager_flow3: FATAL exec %s: %s" % (alias, exc),
                  file=sys.stderr)
            sys.exit(2)
        F[alias] = ns[alias]
    return ns


def drive(func, inp, st):
    CUR["now"] = inp.get("now", 0.0)
    REG.clear()
    try:
        if func == "redact_for_provider":
            ret = F["_redact_for_provider"](*inp.get("values", []),
                                            urls=inp.get("urls", True))
        elif func == "redact_messages":
            ret = F["_redact_messages_for_egress"](inp.get("messages"))
        elif func == "gate_init":
            st["gate"] = []
            ret = None
        elif func == "gate_enter":
            g = GateStateDouble(st)
            ret = F["gate_enter"](g, tuple(inp["key"]), inp.get("token", ""),
                                  inp.get("timeout", 0.0))
            g.save()
        elif func == "gate_leave":
            g = GateStateDouble(st)
            ret = F["gate_leave"](g, tuple(inp["key"]), inp.get("token", ""),
                                  ok=inp.get("ok", False))
            g.save()
        elif func == "manager_init":
            slf = SelfDouble(st)
            ret = F["mgr_init"](slf, external_prefetch_timeout=inp.get("external_prefetch_timeout"))
        elif func == "tool_call_keywords":
            slf = SelfDouble(st)
            ret = F["tool_call_keywords"](slf, mat_provider(inp.get("provider")))
        elif func == "drop_provider":
            slf = SelfDouble(st)
            provs = [mat_provider(p) for p in (st.get("providers") or [])]
            byid = {p._pid: p for p in provs}
            st["providers"] = provs
            st["tool_to_provider"] = {
                t: byid.get(v.get("id"), mat_provider(v)) if isinstance(v, dict) else byid.get(v)
                for t, v in (st.get("tool_to_provider") or {}).items()}
            if "tool_call_keywords_cache" not in st:
                st["tool_call_keywords_cache"] = {}
            ret = F["drop_provider"](slf, mat_provider(inp.get("provider")))
        elif func == "initialize_for_agent":
            slf = SelfDouble(st)
            if "providers" not in st:
                st["providers"] = [mat_provider(p) for p in (inp.get("before") or [])]
            else:
                st["providers"] = [mat_provider(p) for p in st["providers"]]
            before = st["providers"]
            sayfn = (lambda m: seam("say", {"message": m})) if inp.get("say") else None
            ret = F["initialize_for_agent"](slf, say=sayfn, **(inp.get("kwargs") or {}))
        else:
            raise ValueError("unknown func: %s" % func)
    except BaseException as e:
        ret = None
        for exc, spec in CUR["raised"]:
            if e is exc:
                ret = spec
                break
        if ret is None:
            ret = {"__raise__": type(e).__name__, "msg": str(e)}
    return ret


def main():
    extract()
    cases = 0
    mism = 0
    for line in sys.stdin:
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
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["raised"] = []
        if func not in ALL_FUNCS:
            mism += 1
            print("PYERR unknown func: %s" % func)
            continue
        st = copy.deepcopy(inp.get("state") or {})
        ret = drive(func, inp, st)
        want = {"ret": norm(ret), "trace": CUR["trace"], "state": norm(st)}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    if cases == 0:
        print("ORACLE memory_manager_flow3: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE memory_manager_flow3: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
