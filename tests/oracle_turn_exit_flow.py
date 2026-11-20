#!/usr/bin/env python3
"""Oracle for turn_exit_flow — the UNCLAIMED turn-exit FLOW surface that
consumes hermes_cli/turn_exit.py (LIVE authority, py3.14 quarry venv).

hermes_cli/turn_exit.py itself has exactly two defs (turn_exit_code,
credential_failure_flags), BOTH fully claimed by session_kanban_flow2;
they are SEAMED here. The ported remainder lives in the consumer modules:
  hermes_cli/cli_single_query.py: _int_or, _single_query_exit_code,
    _sync_cli_session_id_from_agent, _interrupt_agent_for_signal,
    _configure_quiet_agent, _run_kanban_goal_loop_chat,
    _collect_kanban_task_images, _route_single_query_images
  hermes_cli/gateway_chat_view.py: _pending_preview, _quoted,
    pending_lines, lost_admission, _oneshot_admission, resume_footer,
    GatewayChatView._finite_block (unknown_admissions extracted as a real
    unclaimed helper, bound on the self double)
  hermes_cli/quiet_single_query.py: exit_single_query, take_turn_report_path
  hermes_cli/oneshot.py: _oneshot_exit_code

Drives the LIVE functions with SCRIPTED I/O: defs AST-extracted from the
quarry (no heavy import), module constants seeded, every effect replaced
by a recording stub returning the SAME scripted results the C harness
feeds its universal seam. Compares the ordered DECISION SEQUENCE (trace of
seam calls) plus the return value (and the mutated state window for the
mutation flows). Scripted {"__raise__": cls} raises a REAL exception of
that class (GatewayClientError is the REAL extracted quarry class, a
ValueError); drive() catches BaseException so KeyboardInterrupt/SystemExit
pass through live except-Exception / suppress(Exception) clauses exactly
as the C port propagates them. A MISSING seam key fail-closes (RuntimeError)
on both sides; a scripted null is a valid None result. sys.exit runs REAL
(no seam) and lands in the same SystemExit envelope the C constructs.

Input: stdin JSON lines from t_port_turn_exit_flow:
    {"func": ..., "in": {...}, "out": {"ret":..., "trace":[...][, "state":...]}}
Summary: ORACLE turn_exit_flow: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.
--self-test: drive every fixture case (no C) and report binding/script gaps.
"""
import ast
import copy
import json
import os
import sys
import textwrap
import types
from pathlib import Path, PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "turn_exit_flow_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC_CSQ = os.path.join(QUARRY, "hermes_cli", "cli_single_query.py")
SRC_GCV = os.path.join(QUARRY, "hermes_cli", "gateway_chat_view.py")
SRC_QSQ = os.path.join(QUARRY, "hermes_cli", "quiet_single_query.py")
SRC_ONESHOT = os.path.join(QUARRY, "hermes_cli", "oneshot.py")
SRC_GWCLIENT = os.path.join(QUARRY, "hermes_cli", "gateway_client.py")

# C harness func -> live python name.
PY = {
    "int_or": "_int_or",
    "single_query_exit_code": "_single_query_exit_code",
    "sync_cli_session_id": "_sync_cli_session_id_from_agent",
    "interrupt_agent_for_signal": "_interrupt_agent_for_signal",
    "configure_quiet_agent": "_configure_quiet_agent",
    "run_kanban_goal_loop_chat": "_run_kanban_goal_loop_chat",
    "collect_kanban_task_images": "_collect_kanban_task_images",
    "route_single_query_images": "_route_single_query_images",
    "pending_preview": "_pending_preview",
    "quoted": "_quoted",
    "pending_lines": "pending_lines",
    "lost_admission": "lost_admission",
    "oneshot_admission": "_oneshot_admission",
    "resume_footer": "resume_footer",
    "finite_block": "_finite_block",
    "exit_single_query": "exit_single_query",
    "take_turn_report_path": "take_turn_report_path",
    "oneshot_exit_code": "_oneshot_exit_code",
}
WANTED = set(PY.values()) | {"unknown_admissions"}
STATEFUL = {"sync_cli_session_id", "configure_quiet_agent",
            "collect_kanban_task_images", "finite_block",
            "exit_single_query", "take_turn_report_path"}

CUR = {"in": {}, "trace": [], "scripts": {}, "idx": {}, "statewin": None}


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    if name not in CUR["scripts"]:
        r = {"__raise__": "RuntimeError", "msg": "unscripted seam: " + name}
    else:
        sc = CUR["scripts"][name]
        if isinstance(sc, list):
            i = CUR["idx"].get(name, 0)
            CUR["idx"][name] = i + 1
            r = sc[min(i, len(sc) - 1)] if sc else None
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


EXC_MAP = {
    "OSError": OSError, "IOError": OSError, "EnvironmentError": OSError,
    "WindowsError": OSError,
    "FileNotFoundError": FileNotFoundError, "PermissionError": PermissionError,
    "FileExistsError": FileExistsError, "IsADirectoryError": IsADirectoryError,
    "NotADirectoryError": NotADirectoryError, "TimeoutError": TimeoutError,
    "BlockingIOError": BlockingIOError, "ProcessLookupError": ProcessLookupError,
    "ChildProcessError": ChildProcessError, "InterruptedError": InterruptedError,
    "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError,
    "ValueError": ValueError, "TypeError": TypeError, "KeyError": KeyError,
    "AttributeError": AttributeError, "RuntimeError": RuntimeError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "PythonFinalizationError": getattr(__import__("builtins"), "PythonFinalizationError", RuntimeError),
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "IndexError": IndexError, "LookupError": LookupError, "SyntaxError": SyntaxError,
    "IndentationError": IndentationError, "TabError": TabError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeError": UnicodeError, "NameError": NameError,
    "UnboundLocalError": UnboundLocalError, "AssertionError": AssertionError,
    "StopIteration": StopIteration, "StopAsyncIteration": StopAsyncIteration,
    "ArithmeticError": ArithmeticError, "ZeroDivisionError": ZeroDivisionError,
    "OverflowError": OverflowError, "FloatingPointError": FloatingPointError,
    "MemoryError": MemoryError, "ReferenceError": ReferenceError,
    "SystemError": SystemError, "BufferError": BufferError, "EOFError": EOFError,
    "Exception": Exception, "BaseException": BaseException,
    "KeyboardInterrupt": KeyboardInterrupt, "SystemExit": SystemExit,
    "GeneratorExit": GeneratorExit, "Warning": Warning, "UserWarning": UserWarning,
    "DeprecationWarning": DeprecationWarning, "RuntimeWarning": RuntimeWarning,
    "FutureWarning": FutureWarning, "ResourceWarning": ResourceWarning,
    "BaseExceptionGroup": BaseExceptionGroup, "ExceptionGroup": ExceptionGroup,
}


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        name = r["__raise__"]
        if name == "GatewayClientError":
            raise NS["GatewayClientError"](r.get("msg", ""))
        cls = EXC_MAP.get(name)
        if cls is None:
            cls = type(name, (Exception,), {})
        raise cls(r.get("msg", ""))
    return r


NS = {}  # extraction namespace (set by extract)


class PathDouble:
    """A Path stand-in: PurePosixPath stringification (the port's POSIX
    path domain; Windows Path would flip separators)."""

    def __init__(self, s):
        self.posix = PurePosixPath(s).as_posix() if s is not None else ""

    def __str__(self):
        return self.posix

    def __fspath__(self):
        return self.posix


def norm(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, PathDouble):
        return o.posix
    if isinstance(o, Path):
        return o.as_posix()
    if isinstance(o, types.SimpleNamespace):
        return {k: norm(v) for k, v in vars(o).items()}
    if isinstance(o, (frozenset, set)):
        return sorted(norm(v) for v in o)
    if callable(o):
        return {"__callable__": getattr(o, "__te2_callable__", True)}
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    return str(o)


# ── extraction ──

def _future_module(nodes, src):
    """Prepend `from __future__ import annotations` so extracted signatures
    (HermesCLI / Optional / MutableMapping) do not evaluate at exec time."""
    fut = ast.ImportFrom(module="__future__", names=[ast.alias(name="annotations")], level=0)
    mod = ast.Module(body=[fut] + list(nodes), type_ignores=[])
    ast.fix_missing_locations(mod)
    return compile(mod, src, "exec")


class RecLogger:
    def debug(self, *a, **k): pass
    def info(self, *a, **k): pass
    def warning(self, *a, **k): pass
    def error(self, *a, **k): pass


def extract():
    import asyncio
    import contextlib
    import logging
    import uuid
    from contextlib import suppress
    from typing import Any, Callable, MutableMapping, Optional

    # os double seeded BEFORE def exec: take_turn_report_path's default
    # `environ=os.environ` binds at def-exec time (drive passes explicitly).
    os_double = types.SimpleNamespace(environ={})

    ns = {
        "__name__": "hermes_cli.turn_exit_extracted",
        "json": json, "os": os_double, "sys": sys, "logging": logging,
        "asyncio": asyncio, "uuid": uuid, "contextlib": contextlib,
        "suppress": suppress, "Path": PathDouble,
        "Any": Any, "Callable": Callable, "MutableMapping": MutableMapping,
        "Optional": Optional,
        "logger": RecLogger(),
    }
    sources = [SRC_CSQ, SRC_GCV, SRC_QSQ, SRC_ONESHOT]
    trees = {}
    for src in sources:
        text = open(src, encoding="utf-8").read()
        trees[src] = (ast.parse(text), text)
        # module constants (plain-Name Assign / AnnAssign only)
        for node in trees[src][0].body:
            if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
                try:
                    exec(_future_module([node], src), ns)
                except Exception:
                    pass
            elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
                try:
                    exec(_future_module([node], src), ns)
                except Exception:
                    pass
    # the module-level `logger = logging.getLogger(...)` Assigns overwrite
    # the seeded recorder — re-set it AFTER the constant loops.
    ns["logger"] = RecLogger()
    # top-level wanted defs
    for src in sources:
        tree, _ = trees[src]
        defs = [n for n in tree.body
                if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name in WANTED]
        if defs:
            exec(_future_module(defs, src), ns)
    # GatewayChatView methods (_finite_block + the unknown_admissions helper)
    tree, text = trees[SRC_GCV]
    methods = []
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "GatewayChatView":
            for m in node.body:
                if isinstance(m, ast.FunctionDef) and m.name in ("_finite_block", "unknown_admissions"):
                    seg = ast.get_source_segment(text, m)
                    m2 = ast.parse(textwrap.dedent(seg)).body[0]
                    m2.decorator_list = []
                    methods.append(m2)
    if methods:
        exec(_future_module(methods, SRC_GCV), ns)
    # the REAL GatewayClientError class (ValueError base — quarry-verified)
    gtext = open(SRC_GWCLIENT, encoding="utf-8").read()
    gtree = ast.parse(gtext)
    for node in gtree.body:
        if isinstance(node, ast.ClassDef) and node.name == "GatewayClientError":
            exec(_future_module([node], SRC_GWCLIENT), ns)
    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    for k in ("GatewayClientError", "_PREVIEW_CHARS", "_UNKNOWN_OUTCOME",
              "KANBAN_WORKER_EXIT_TRAILER", "TURN_REPORT_FILE_ENV",
              "_INTERRUPTED_EXIT_CODE"):
        if k not in ns:
            print("FATAL missing constant/class: %s" % k)
            sys.exit(2)
    global NS
    NS = ns
    return ns


# ── recording stubs ──

def make_import_module(modname, value_attrs=None, fn_attrs=None):
    """value_attrs {attr: seam_name}: getattr fires the seam and RETURNS its
    scripted result (the reasons sets). fn_attrs {attr: (seam_name, fn)}:
    getattr fires the import seam and returns the recorder. NO __path__ on
    leaf stubs (a package __getattr__ would double-fire under
    _handle_fromlist's hasattr probe)."""
    value_attrs = value_attrs or {}
    fn_attrs = fn_attrs or {}

    class M(types.ModuleType):
        def __getattr__(self, name):
            if name in value_attrs:
                return seam(value_attrs[name], {})
            if name in fn_attrs:
                ise, fn = fn_attrs[name]
                seam(ise, {})
                return fn
            raise AttributeError(name)

    m = M(modname)
    sys.modules[modname] = m
    return m


def ensure_pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


class ConnCM:
    def __enter__(self):
        return seam("connect_closing_enter", {})

    def __exit__(self, et, ev, tb):
        r = seam("connect_closing_exit", {})
        return bool(r)   # truthy suppresses; a raise masks the body raise


def _ns_proj(o):
    return vars(o) if isinstance(o, types.SimpleNamespace) else o


def float_env_rec(name, default):
    return seam("float_env", {"name": name, "default": default})


def goal_q_rec(cli, first_response, run_turn=None, log=None):
    return seam("run_kanban_goal_loop_q", {
        "cli": _ns_proj(cli), "first_response": first_response,
        "run_turn": {"__callable__": "run_turn"} if run_turn is not None else None,
        "log": {"__callable__": "log"} if log is not None else None})


def kanban_task_id_rec():
    return seam("kanban_task_id", {})


def turn_exit_code_rec(result, *, kanban_worker, credentials_rate_limited=False,
                       credentials_terminal=False, transient_reasons=None,
                       terminal_reasons=None):
    return seam("turn_exit_code", {
        "result": result, "kanban_worker": kanban_worker,
        "credentials_rate_limited": credentials_rate_limited,
        "credentials_terminal": credentials_terminal,
        "transient_reasons": transient_reasons,
        "terminal_reasons": terminal_reasons})


def request_hard_interrupt_rec(agent, message):
    return seam("request_hard_interrupt", {"agent": agent, "message": message})


def extract_refs_rec(body):
    r = seam("extract_image_refs", {"body": body})
    return tuple(r) if isinstance(r, list) else r


def connect_closing_rec(*a, **k):
    seam("connect_closing", {})
    return ConnCM()


def get_task_rec(conn, task_id):
    r = seam("get_task", {"conn": conn, "task_id": task_id})
    return types.SimpleNamespace(**r) if isinstance(r, dict) else r


def build_parts_rec(query, paths, image_urls=None):
    r = seam("build_native_content_parts",
             {"query": query, "paths": paths, "image_urls": image_urls})
    return tuple(r) if isinstance(r, list) else r


def decide_mode_rec(provider, model, config, requested_provider=None):
    return seam("decide_image_input_mode", {
        "provider": provider, "model": model, "config": config,
        "requested_provider": requested_provider})


def load_config_rec():
    return seam("load_config", {})


def strip_ansi_rec(text):
    return seam("strip_ansi", {"text": text})


def t_rec(key, lang=None, **kw):
    return seam("t", {"key": key})


def profile_rec():
    return seam("get_active_profile_name", {})


def rec_print(*a, file=None, flush=False, end=None, sep=None):
    text = a[0] if len(a) == 1 else (sep or " ").join(str(x) for x in a)
    return seam("print", {"text": text,
                          "file": "stderr" if file is sys.stderr else "stdout",
                          "flush": flush})


def install_stubs(ns):
    # module globals (cli_single_query top-level imports)
    ns["request_hard_interrupt"] = request_hard_interrupt_rec
    ns["time"] = types.SimpleNamespace(sleep=lambda s: seam("time_sleep", {"seconds": s}))
    ns["print"] = rec_print
    # cli facade (leaf, no __path__): value attrs return the script; fn
    # attrs return recorders. The claimed sibling _run_kanban_goal_loop_q
    # is an inert recorder (never invokes run_turn/log).
    make_import_module("cli",
        value_attrs={
            "_TERMINAL_PROVIDER_REASONS": "import__TERMINAL_PROVIDER_REASONS",
            "_TRANSIENT_PROVIDER_REASONS": "import__TRANSIENT_PROVIDER_REASONS",
        },
        fn_attrs={
            "_float_env": ("import__float_env", float_env_rec),
            "_run_kanban_goal_loop_q": ("import__run_kanban_goal_loop_q", goal_q_rec),
        })
    agent = ensure_pkg("agent")
    make_import_module("agent.kanban_turn_recovery",
        fn_attrs={"kanban_task_id": ("import_kanban_task_id", kanban_task_id_rec)})
    make_import_module("agent.image_routing", fn_attrs={
        "extract_image_refs": ("import_extract_image_refs", extract_refs_rec),
        "build_native_content_parts": ("import_build_native_content_parts", build_parts_rec),
        "decide_image_input_mode": ("import_decide_image_input_mode", decide_mode_rec),
    })
    make_import_module("agent.i18n", fn_attrs={"t": ("import_t", t_rec)})
    hermes_cli = ensure_pkg("hermes_cli")
    kb = make_import_module("hermes_cli.kanban_db",
        fn_attrs={"get_task": ("import_get_task", get_task_rec)})
    kbc = make_import_module("hermes_cli.kanban_db_connect",
        fn_attrs={"connect_closing": ("import_connect_closing", connect_closing_rec)})
    # `from hermes_cli import kanban_db / kanban_db_connect` fires NO seam:
    # pre-set the package attributes (a package __getattr__ would double-fire).
    hermes_cli.kanban_db = kb
    hermes_cli.kanban_db_connect = kbc
    make_import_module("hermes_cli.turn_exit",
        fn_attrs={"turn_exit_code": ("import_turn_exit_code", turn_exit_code_rec)})
    make_import_module("hermes_cli.config",
        fn_attrs={"load_config": ("import_load_config", load_config_rec)})
    make_import_module("hermes_cli.profiles",
        fn_attrs={"get_active_profile_name": ("import_get_active_profile_name", profile_rec)})
    tools = ensure_pkg("tools")
    make_import_module("tools.ansi_strip",
        fn_attrs={"strip_ansi": ("import_strip_ansi", strip_ansi_rec)})


# ── drive ──

def drive(ns, func, inp):
    if func not in PY:
        print("FATAL unknown func: %s" % func)
        sys.exit(2)
    name = PY[func]
    f = ns[name]
    # per-case environ double (direct state reads; pop mutates)
    env = dict(inp.get("environ") or {})
    ns["os"].environ = env

    if func == "int_or":
        return f(inp.get("value"), inp.get("default"))
    if func == "single_query_exit_code":
        return f(inp.get("result"),
                 credentials_rate_limited=inp.get("credentials_rate_limited", False),
                 credentials_terminal=inp.get("credentials_terminal", False))
    if func == "sync_cli_session_id":
        cli_d = copy.deepcopy(inp.get("cli") or {})
        cli = types.SimpleNamespace(**cli_d)
        if isinstance(cli.agent, dict):
            cli.agent = types.SimpleNamespace(**cli.agent)
        CUR["statewin"] = cli
        f(cli)
        return None
    if func == "interrupt_agent_for_signal":
        f(inp.get("agent"), inp.get("signum", 0))
        return None
    if func == "configure_quiet_agent":
        agent = types.SimpleNamespace(**copy.deepcopy(inp.get("agent") or {}))
        CUR["statewin"] = agent
        f(agent)
        return None
    if func == "run_kanban_goal_loop_chat":
        cli_d = inp.get("cli")
        # null cli materializes to an empty double (matches the harness's
        # empty_obj); cli is only passed through to the inert recorder.
        cli = types.SimpleNamespace(**cli_d) if isinstance(cli_d, dict) else types.SimpleNamespace()
        f(cli, inp.get("first_response", ""))
        return None
    if func == "collect_kanban_task_images":
        images = list(inp.get("images") or [])
        CUR["statewin"] = {"images": images, "environ": env}
        return f(images)
    if func == "route_single_query_images":
        cli = types.SimpleNamespace(**copy.deepcopy(inp.get("cli") or {}))
        cli._preprocess_images_with_vision = lambda query, images, announce=False: seam(
            "preprocess_images_with_vision",
            {"query": query, "images": images, "announce": announce})
        return f(cli, inp.get("query"), inp.get("effective_query"),
                 list(inp.get("images") or []), list(inp.get("image_urls") or []))
    if func == "pending_preview":
        return f(inp.get("text"))
    if func == "quoted":
        return f(inp.get("row") or {}, inp.get("fallback", ""))
    if func == "pending_lines":
        return f(inp.get("pending") or [], interactive=inp.get("interactive", True))
    if func == "lost_admission":
        return f(inp.get("snapshot"), inp.get("admission_id", ""))
    if func == "oneshot_admission":
        return f(inp.get("receipt"))
    if func == "resume_footer":
        return f(inp.get("session_id", ""))
    if func == "finite_block":
        self_d = copy.deepcopy(inp.get("self") or {})
        slf = types.SimpleNamespace(
            pending=self_d.get("pending", []),
            completions=dict(self_d.get("completions", {})),
            prompts=dict(self_d.get("prompts", {})),
            gaps=self_d.get("gaps", 0))
        slf.unknown_admissions = types.MethodType(ns["unknown_admissions"], slf)
        slf._finite_block = types.MethodType(ns["_finite_block"], slf)
        CUR["statewin"] = {k: getattr(slf, k)
                           for k in ("pending", "completions", "prompts", "gaps")}
        return slf._finite_block(inp.get("admission", ""))
    if func == "exit_single_query":
        CUR["statewin"] = {"environ": env}
        f(inp.get("code", 0))   # real sys.exit raises SystemExit
        return None
    if func == "take_turn_report_path":
        CUR["statewin"] = {"environ": env}
        return f(env)
    if func == "oneshot_exit_code":
        return f(inp.get("response"), inp.get("result") or {})
    raise ValueError("unhandled func: %s" % func)


def _numnorm(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: _numnorm(v) for k, v in o.items()}
    if isinstance(o, list):
        return [_numnorm(v) for v in o]
    return o


def canon(o):
    return json.loads(json.dumps(_numnorm(o), sort_keys=True, ensure_ascii=False, default=str))


SEAM_NAMES = [
    "import__TERMINAL_PROVIDER_REASONS", "import__TRANSIENT_PROVIDER_REASONS",
    "import__float_env", "float_env", "import__run_kanban_goal_loop_q",
    "run_kanban_goal_loop_q", "import_kanban_task_id", "kanban_task_id",
    "import_turn_exit_code", "turn_exit_code", "request_hard_interrupt",
    "time_sleep", "import_extract_image_refs", "extract_image_refs",
    "import_connect_closing", "connect_closing", "connect_closing_enter",
    "connect_closing_exit", "import_get_task", "get_task",
    "import_build_native_content_parts", "import_decide_image_input_mode",
    "import_load_config", "load_config", "decide_image_input_mode",
    "build_native_content_parts", "preprocess_images_with_vision",
    "import_strip_ansi", "strip_ansi", "import_t", "t",
    "import_get_active_profile_name", "get_active_profile_name", "print",
]


def self_test(ns):
    """Drive every fixture case live (no C): surface binding bugs / script gaps."""
    import faulthandler
    faulthandler.dump_traceback_later(120, exit=True)
    fixture = _REPO / "tests" / "fixtures" / "turn_exit_flow.jsonl"
    cases = [json.loads(l) for l in fixture.read_text(encoding="utf-8").splitlines() if l.strip()]
    fixture_funcs = {c["func"] for c in cases}
    if fixture_funcs != set(PY):
        print("SELFTEST FATAL func set drift: fixture-only=%s map-only=%s"
              % (sorted(fixture_funcs - set(PY)), sorted(set(PY) - fixture_funcs)))
        sys.exit(2)
    counts = {}
    for c in cases:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("SELFTEST FATAL <5 cases for: %s" % thin)
        sys.exit(2)
    install_stubs(ns)
    bad = 0
    failclosed = 0
    names = set(PY.values()) | set(PY) | set(SEAM_NAMES) | {"import_" + n for n in SEAM_NAMES}
    for c in cases:
        inp = c["in"]
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["statewin"] = None
        try:
            drive(ns, c["func"], inp)
        except BaseException as ex:
            spec = "%s: %s" % (type(ex).__name__, ex)
            if type(ex).__name__ == "AttributeError" and any(("'%s'" % n) in str(ex) for n in names):
                print("SELFTEST BINDING %s: %s" % (c["func"], spec))
                bad += 1
            elif type(ex).__name__ == "NameError":
                print("SELFTEST NAMEERROR %s: %s" % (c["func"], spec))
                bad += 1
        if any(e.get("ret", {}).get("msg", "").startswith("unscripted seam:")
               for e in CUR["trace"] if isinstance(e.get("ret"), dict)):
            failclosed += 1
    faulthandler.cancel_dump_traceback_later()
    print("SELFTEST %d cases, %d binding/name errors, %d cases hit fail-closed seams"
          % (len(cases), bad, failclosed))
    sys.exit(1 if bad else 0)


def main():
    if "--self-test" in sys.argv:
        self_test(extract())
        return
    ns = extract()
    install_stubs(ns)
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
        CUR["in"] = inp
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["statewin"] = None
        try:
            ret = drive(ns, func, inp)
            want = {"ret": norm(ret), "trace": CUR["trace"]}
        except BaseException as ex:
            want = {"ret": {"__raise__": type(ex).__name__, "msg": str(ex)},
                    "trace": CUR["trace"]}
        if func in STATEFUL:
            want["state"] = norm(CUR["statewin"]) if CUR["statewin"] is not None else {}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE turn_exit_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE turn_exit_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()