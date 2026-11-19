#!/usr/bin/env python3
"""Oracle for cli_py_flow.c — the UNCLAIMED quarry ROOT cli.py FLOW cores +
leaves (LIVE authority): the output-history configure/suspend/replay cluster,
the slash-text leaves, the skill-command cache leaves, the -q seed gate, the
two input-sentinel __str__ leaves, and the HermesCLI slash/skill/quick-command
dispatch cluster.

NOTE: the task brief names "hermes_cli/cli.py"; the quarry has NO such file —
the CLI facade is the ROOT cli.py (every existing PoP claim and watercup gate
credits "cli.py"). This oracle extracts from that real defining module.

Drives the LIVE defs with SCRIPTED I/O: defs are AST-extracted from the quarry
(no heavy cli import), module constants seeded explicitly, and every claimed
sibling (process_command, _console_print, get_skill_bundles,
_get_plugin_cmd_handler_names) plus every foreign collaborator (cli_render
helpers, prompt_toolkit print/paint, agent.skill_commands / skill_bundles,
gateway_plugin_commands, security_advisories, the vault/tools callback
setters, subprocess, redact, plugin dispatch/choices, i18n t, ChatConsole,
cprint, COMMANDS, unknown_command_lines, the module logger) replaced by
recording stubs returning the SAME scripted results the C harness feeds its
universal seam. Ported-to-ported calls compose REAL on both sides, so their
internal seam events appear in both traces.

Modeling (see the port header): module globals travel in the state object;
_PaintedLine entries are {"__painted__":true,"text","width"} doubles interned
by text between the history deque and the line seams (live repaints the
recorded objects); the CLI receiver projects to {"__cli__":true}; GONE projects
to {"__gone__":true} (the run/run_skill recorders return the real GONE object
for that script so `is GONE` agrees); bind_asker is an enter/exit CM seam pair;
_PT_ANSI / Style are pure inline wrappers; _ACCENT is the skin fallback ANSI.

Fail-closed: a missing/empty seam script RETURNS a RuntimeError raise spec
(recorded as the event ret, then raised), matching the C harness. Scripted
raises round-trip BARE via identity match; unscripted live raises become
{__raise__, msg=str(exc)}. The BaseException family passes through every
`except Exception` site; `except TimeoutExpired` / `except CatalogUnavailable`
swallow exactly their kinds.

Input: stdin JSON lines from t_port_cli_py_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "state":{...}}}
Summary: ORACLE cli_py_flow: N cases, M mismatches. Fail-closed (exit 2) when
extraction misses defs or zero cases were compared.
"""
import ast
import asyncio
import builtins
import collections
import copy
import json
import os
import sys
import types
import __future__
from contextlib import contextmanager
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not QUARRY.is_dir():
    print("ORACLE cli_py_flow: quarry not found at %s" % QUARRY, file=sys.stderr)
    sys.exit(2)
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "cli_py_flow_home"))

SRC = str(QUARRY / "cli.py")
SRC_TEXT = open(SRC, encoding="utf-8").read()
ANN_FLAG = __future__.annotations.compiler_flag

# harness dispatch name -> live def name
PY = {
    "configure_output_history": "_configure_output_history",
    "suspend_output_history": "_suspend_output_history",
    "replay_output_history": "_replay_output_history",
    "looks_like_slash_command": "_looks_like_slash_command",
    "slash_args": "_slash_args",
    "ensure_skill_commands": "_ensure_skill_commands",
    "session_skill_commands": "_session_skill_commands",
    "should_seed_interactive": "_should_seed_interactive",
    "voice_input_message_str": "_VoiceInputMessage.__str__",
    "seeded_query_message_str": "_SeededQueryMessage.__str__",
    "install_tool_callbacks": "_install_tool_callbacks",
    "show_security_advisories": "_show_security_advisories",
    "finalize_preloaded_skills": "finalize_preloaded_skills",
    "slash_handler": "_slash_handler",
    "process_unregistered_slash": "_process_unregistered_slash",
    "run_quick_command": "_run_quick_command",
    "run_plugin_slash_command": "_run_plugin_slash_command",
    "queue_skill_message": "_queue_skill_message",
    "run_skill_bundle_command": "_run_skill_bundle_command",
    "queue_loaded_skills": "_queue_loaded_skills",
    "run_skill_slash_command": "_run_skill_slash_command",
    "expand_slash_prefix": "_expand_slash_prefix",
}
TOP_TARGETS = [
    "_configure_output_history", "_suspend_output_history", "_replay_output_history",
    "_looks_like_slash_command", "_slash_args", "_ensure_skill_commands",
    "_session_skill_commands", "_should_seed_interactive",
]
METHOD_TARGETS = [
    "_install_tool_callbacks", "_show_security_advisories", "finalize_preloaded_skills",
    "_slash_handler", "_process_unregistered_slash", "_run_quick_command",
    "_run_plugin_slash_command", "_queue_skill_message", "_run_skill_bundle_command",
    "_queue_loaded_skills", "_run_skill_slash_command", "_expand_slash_prefix",
]
SENTINEL_CLASSES = ["_VoiceInputMessage", "_SeededQueryMessage"]

CUR = {"scripts": {}, "idx": {}, "trace": [], "state": None, "raised": [], "intern": {}}
CLI_MARK = {"__cli__": True}
GONE_MARK = {"__gone__": True}
ACCENT = "\x1b[1;38;2;255;215;0m"
DIM = "\x1b[2;3m"
RST = "\x1b[0m"

BASE_EXC = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit, "CancelledError": asyncio.CancelledError}
BUILTIN_EXC = {
    "RuntimeError": RuntimeError, "ValueError": ValueError, "TypeError": TypeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "IndexError": IndexError,
    "Exception": Exception, "StopIteration": StopIteration, "OSError": OSError,
    "EOFError": EOFError, "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "UnicodeDecodeError": UnicodeDecodeError, "NameError": NameError, "LookupError": LookupError,
    "ArithmeticError": ArithmeticError, "ZeroDivisionError": ZeroDivisionError,
    "AssertionError": AssertionError, "SyntaxError": SyntaxError, "SystemError": SystemError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "FileNotFoundError": FileNotFoundError, "PermissionError": PermissionError,
    "TimeoutError": TimeoutError, "ConnectionError": ConnectionError,
    "BufferError": BufferError, "ReferenceError": ReferenceError, "Warning": Warning,
    "UnboundLocalError": UnboundLocalError, "OverflowError": OverflowError,
}
CUSTOM_EXC = {}


def make_exc(kind, msg):
    cls = BASE_EXC.get(kind) or CUSTOM_EXC.get(kind) or BUILTIN_EXC.get(kind)
    if cls is None:
        cls = type(str(kind), (Exception,), {})
    return cls(msg)


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        exc = make_exc(r["__raise__"], r.get("msg", ""))
        CUR["raised"].append((exc, r))
        raise exc
    return r


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    scripts = CUR["scripts"]
    if name not in scripts:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    else:
        sc = scripts[name]
        if isinstance(sc, list):
            if not sc:
                r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
            else:
                i = CUR["idx"].get(name, 0)
                CUR["idx"][name] = i + 1
                r = sc[min(i, len(sc) - 1)]
        else:
            r = sc
    # Snapshot args at call time (the C harness json_copies args into the
    # event): mutable painted-line doubles later repainted by the flow must
    # NOT retroactively change the recorded args.
    CUR["trace"].append({"ev": name, "args": norm(args), "ret": r})
    return maybe_raise(r)


def norm(o):
    if isinstance(o, _PaintedLineD):
        return {"__painted__": True, "text": str(o), "width": o.width}
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, QueueDouble):
        return {"__queue__": True}
    if isinstance(o, ThreadDouble):
        return {"__thread__": True}
    if o is GPC.GONE:
        return dict(GONE_MARK)
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, (set, frozenset)):
        return sorted(norm(v) for v in o)
    if isinstance(o, BaseException):
        return str(o)
    return str(o)


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


def mk(name, proj, build=None):
    def f(*a, **k):
        r = seam(name, proj(a, k))
        return build(r) if build is not None else r
    f.__name__ = name
    return f


# ── attribute doubles ──

class _PaintedLineD(str):
    width = None


class ANSID(str):
    """_PT_ANSI double: pure str-preserving wrapper (no seam)."""


class TTYDouble:
    def __init__(self, name):
        self._name = name

    def isatty(self):
        return seam(self._name, {})


class QueueDouble:
    def put(self, msg):
        return seam("pending_input.put", {"msg": msg})


class ThreadDouble:
    def join(self, timeout=None):
        return seam("preload_thread.join", {"timeout": timeout})


class OutputDouble:
    def __init__(self, rows, columns):
        self.rows = rows
        self.columns = columns

    def get_size(self):
        r = seam("output.get_size", {})
        if isinstance(r, dict):
            return types.SimpleNamespace(**r)
        return r


class ChatConsoleD:
    def print(self, text):
        return seam("ChatConsole.print", {"text": text})


class BindAskerCM:
    def __init__(self, asker):
        self.asker = asker

    def __enter__(self):
        return seam("bind_asker.enter", {"asker": self.asker})

    def __exit__(self, et, ev, tb):
        exc = {"kind": et.__name__, "msg": str(ev)} if et is not None else None
        seam("bind_asker.exit", {"exc": exc})
        return False


class RecLogger:
    def debug(self, *a, **k): pass
    def info(self, *a, **k): pass
    def error(self, *a, **k): pass
    def warning(self, *a, **k):
        return seam("logger.warning", {"fmt": a[0], "args": list(a[1:])})


def mat_line(x):
    if isinstance(x, str):
        return x
    if isinstance(x, dict) and x.get("__painted__"):
        text = x.get("text", "")
        intern = CUR["intern"]
        if text in intern:
            return intern[text]
        pl = _PaintedLineD(text)
        pl.width = x.get("width")
        intern[text] = pl
        return pl
    return x


def mat_lines(r):
    if isinstance(r, list):
        return [mat_line(x) for x in r]
    return r


def proj_line(x):
    if isinstance(x, _PaintedLineD):
        return {"__painted__": True, "text": str(x), "width": x.width}
    return str(x)


def gone_build(r):
    if r == GONE_MARK:
        return GPC.GONE
    return r


def ns_build(r):
    if isinstance(r, dict):
        return types.SimpleNamespace(**r)
    return r


def print_rec(*a, **k):
    file = k.get("file")
    fp = "stderr" if file is sys.stderr else ("stdout" if file is sys.stdout else None)
    return seam("print", {"args": list(a), "file": fp, "flush": k.get("flush")})


def proj_output(o):
    return {"rows": getattr(o, "rows", None), "columns": getattr(o, "columns", None)}


def seam_bindings():
    return {
        "print": print_rec,
        "_t": mk("t", lambda a, k: {"key": a[0], "kwargs": k}),
        "_cprint": mk("cprint", lambda a, k: {"text": a[0]}),
        "ChatConsole": ChatConsoleD,
        "_rich_text_from_ansi": mk("rich_text_from_ansi", lambda a, k: {"text": a[0]}),
        "_coerce_output_history_limit": mk("coerce_output_history_limit", lambda a, k: {"value": a[0]}),
        "_output_history_lines": mk("output_history_lines", lambda a, k: {}, mat_lines),
        "_output_tail_fitting": mk("output_tail_fitting",
                                   lambda a, k: {"lines": a[0], "rows": a[1], "columns": a[2], "painted": a[3]},
                                   mat_lines),
        "_line_rows": mk("line_rows", lambda a, k: {"line": str(a[0]), "columns": a[1]}),
        "_set_chrome_floor": mk("set_chrome_floor", lambda a, k: {"rows": a[0]}),
        "_add_suspect_rows": mk("add_suspect_rows", lambda a, k: {"rows": a[0]}),
        "_painted_columns": mk("painted_columns", lambda a, k: {}),
        "_pt_print": mk("pt_print", lambda a, k: {"text": str(a[0])}),
        "_PT_ANSI": ANSID,
        "_PaintedLine": _PaintedLineD,
        "_ACCENT": ACCENT, "_DIM": DIM, "_RST": RST,
        "get_interactive_skill_commands": mk("get_interactive_skill_commands", lambda a, k: {}),
        "set_sudo_password_callback": mk("set_sudo_password_callback", lambda a, k: {"callback": a[0]}),
        "set_approval_callback": mk("set_approval_callback", lambda a, k: {"callback": a[0]}),
        "set_secret_capture_callback": mk("set_secret_capture_callback", lambda a, k: {"callback": a[0]}),
        "build_bundle_invocation_message": mk("build_bundle_invocation_message",
                                              lambda a, k: {"command": a[0], "instruction": a[1], "task_id": k.get("task_id")}),
        "build_skill_invocation_message": mk("build_skill_invocation_message",
                                             lambda a, k: {"command": a[0], "rest": a[1], "task_id": k.get("task_id")}),
        "get_skill_bundles": mk("get_skill_bundles", lambda a, k: {}),
        "_get_plugin_cmd_handler_names": mk("get_plugin_cmd_handler_names", lambda a, k: {}),
        "logger": RecLogger(),
        "deque": collections.deque,
    }


GPC = types.ModuleType("hermes_cli.gateway_plugin_commands")
SA_MOD = types.ModuleType("hermes_cli.security_advisories")


def install_module_stubs():
    hcli = types.ModuleType("hermes_cli")
    hcli.__path__ = []
    sys.modules["hermes_cli"] = hcli

    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    # agent package + skill_commands / vault / redact
    agent = mod("agent")
    agent.__path__ = []
    sc = mod("agent.skill_commands",
             get_interactive_skill_commands=mk("get_interactive_skill_commands", lambda a, k: {}),
             format_missing_skills=mk("format_missing_skills", lambda a, k: {"missing": a[0]}),
             build_stacked_skill_invocation_message=mk(
                 "build_stacked_skill_invocation_message",
                 lambda a, k: {"commands": a[0], "instruction": a[1], "task_id": k.get("task_id")}),
             split_stacked_skill_commands=mk(
                 "split_stacked_skill_commands",
                 lambda a, k: {"rest": a[0], "interactive": k.get("interactive")}))
    agent.skill_commands = sc
    vb = mod("agent.vault_backends")
    vb.__path__ = []
    agent.vault_backends = vb
    unlock = mod("agent.vault_backends.unlock",
                 set_unlock_prompt_callback=mk("set_unlock_prompt_callback", lambda a, k: {"callback": a[0]}),
                 set_save_login_prompt_callback=mk("set_save_login_prompt_callback", lambda a, k: {"callback": a[0]}),
                 set_code_prompt_callback=mk("set_code_prompt_callback", lambda a, k: {"callback": a[0]}))
    vb.unlock = unlock
    redact = mod("agent.redact",
                 redact_sensitive_text=mk("redact_sensitive_text", lambda a, k: {"text": a[0]}))
    agent.redact = redact

    # gateway_plugin_commands (GONE sentinel + CatalogUnavailable)
    GPC.GONE = object()
    GPC.CatalogUnavailable = type("CatalogUnavailable", (Exception,), {})
    CUSTOM_EXC["CatalogUnavailable"] = GPC.CatalogUnavailable
    GPC.skills = mk("gateway_plugin_commands.skills", lambda a, k: {"cli": CLI_MARK})
    GPC.lookup = mk("gateway_plugin_commands.lookup", lambda a, k: {"cli": CLI_MARK, "name": a[1]})
    GPC.run = mk("gateway_plugin_commands.run",
                 lambda a, k: {"cli": CLI_MARK, "command": a[1], "args": a[2]}, gone_build)
    GPC.run_skill = mk("gateway_plugin_commands.run_skill",
                       lambda a, k: {"cli": CLI_MARK, "command": a[1], "rest": a[2]}, gone_build)
    GPC.report_gone = mk("gateway_plugin_commands.report_gone",
                         lambda a, k: {"name": a[0], "known": list(a[1:])})
    sys.modules["hermes_cli.gateway_plugin_commands"] = GPC
    hcli.gateway_plugin_commands = GPC

    SA_MOD.detect_compromised = mk("detect_compromised", lambda a, k: {})
    SA_MOD.startup_banner = mk("startup_banner", lambda a, k: {"advisories": a[0]})
    sys.modules["hermes_cli.security_advisories"] = SA_MOD
    hcli.security_advisories = SA_MOD

    pcd = mod("hermes_cli.plugin_command_dispatch",
              invoke_plugin_command=mk("invoke_plugin_command",
                                       lambda a, k: {"handler": a[0], "args": a[1], "source": k.get("source"),
                                                     "session_id": k.get("session_id"), "session_key": k.get("session_key"),
                                                     "platform": k.get("platform")}))
    hcli.plugin_command_dispatch = pcd
    plugins = mod("hermes_cli.plugins",
                  get_plugin_command_handler=mk("get_plugin_command_handler", lambda a, k: {"command": a[0]}),
                  resolve_plugin_command_result=mk("resolve_plugin_command_result", lambda a, k: {"result": a[0]}))
    hcli.plugins = plugins
    choices = mod("hermes_cli.plugin_choices",
                  bind_asker=lambda asker: BindAskerCM(asker),
                  clarify_asker=mk("clarify_asker", lambda a, k: {"callback": a[0]}))
    hcli.plugin_choices = choices

    commands = types.ModuleType("hermes_cli.commands")

    def commands_getattr(name):
        if name == "COMMANDS":
            return seam("COMMANDS", {})
        raise AttributeError(name)

    commands.__getattr__ = commands_getattr
    sys.modules["hermes_cli.commands"] = commands
    hcli.commands = commands

    unk = mod("hermes_cli.cli_unknown_command",
              unknown_command_lines=mk("unknown_command_lines",
                                       lambda a, k: {"command": a[0], "known": sorted(a[1])}))
    hcli.cli_unknown_command = unk
    compat = mod("hermes_cli._subprocess_compat",
                 windows_hide_flags=mk("windows_hide_flags", lambda a, k: {}))
    hcli._subprocess_compat = compat

    tools = mod("tools")
    tools.__path__ = []
    envs = mod("tools.environments")
    envs.__path__ = []
    tools.environments = envs
    local = mod("tools.environments.local",
                build_subprocess_env=mk("build_subprocess_env", lambda a, k: {}))
    envs.local = local

    # subprocess double (run recorder + TimeoutExpired kind)
    subp = types.ModuleType("subprocess")
    subp.TimeoutExpired = type("TimeoutExpired", (Exception,), {})
    CUSTOM_EXC["TimeoutExpired"] = subp.TimeoutExpired
    subp.run = mk("subprocess.run",
                  lambda a, k: {"command": a[0],
                                **{kk: k[kk] for kk in ("shell", "capture_output", "text", "encoding",
                                                        "errors", "timeout", "env", "creationflags", "check")
                                   if kk in k}},
                  ns_build)
    sys.modules["subprocess"] = subp

    # prompt_toolkit paint + Style
    pt = mod("prompt_toolkit")
    pt.__path__ = []
    renderer = mod("prompt_toolkit.renderer",
                   print_formatted_text=mk("paint_formatted_text",
                                           lambda a, k: {"output": proj_output(a[0]), "text": str(a[1]), "style": a[2]}))
    pt.renderer = renderer
    styles = mod("prompt_toolkit.styles", Style=lambda x: [])
    pt.styles = styles


# ── extraction ──

def extract():
    ns = {
        "__name__": "cli_py_flow_extracted",
        "__builtins__": builtins.__dict__,
        "copy": copy, "os": os, "sys": sys,
        "contextmanager": contextmanager,
        "collections": collections,
    }
    sys.modules["cli_py_flow_extracted"] = types.ModuleType("cli_py_flow_extracted")
    ns.update(seam_bindings())
    tree = ast.parse(SRC_TEXT)

    def exec_node(node):
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=ANN_FLAG), ns)

    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in SENTINEL_CLASSES:
            exec_node(node)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in TOP_TARGETS:
            n = copy.copy(node)
            n.decorator_list = []
            exec_node(n)
    ns["_suspend_output_history"] = contextmanager(ns["_suspend_output_history"])
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "HermesCLI":
            for m in node.body:
                if isinstance(m, ast.FunctionDef) and m.name in METHOD_TARGETS:
                    mm = copy.copy(m)
                    mm.decorator_list = []
                    exec_node(mm)
    ns["logger"] = RecLogger()

    missing = [n for n in TOP_TARGETS + METHOD_TARGETS + SENTINEL_CLASSES if n not in ns]
    if missing:
        print("ORACLE cli_py_flow: FATAL missing defs: %s" % sorted(missing), file=sys.stderr)
        sys.exit(2)
    return ns


# ── drive helpers ──

def setup_module_state(ns, inp):
    st = inp.get("state") or {}
    ns["_OUTPUT_HISTORY_ENABLED"] = st.get("enabled", True)
    ns["_OUTPUT_HISTORY_MAX_LINES"] = st.get("max_lines", 200)
    ns["_OUTPUT_HISTORY_REPLAYING"] = st.get("replaying", False)
    ns["_OUTPUT_HISTORY_SUPPRESSED"] = st.get("suppressed", False)
    CUR["intern"] = {}
    hist = [mat_line(x) for x in (st.get("history") or [])]
    ml = ns["_OUTPUT_HISTORY_MAX_LINES"]
    ns["_OUTPUT_HISTORY"] = collections.deque(hist, maxlen=ml if isinstance(ml, int) else None)
    ns["_skill_commands"] = st.get("skill_commands")


def hist_echo(ns):
    return [proj_line(x) for x in ns["_OUTPUT_HISTORY"]]


def mk_self(ns, inp):
    fields = copy.deepcopy(inp.get("self") or {})
    if "_pending_input" in fields:
        v = fields["_pending_input"]
        fields["_pending_input"] = QueueDouble() if isinstance(v, dict) else v
    if "_preload_skills_thread" in fields:
        v = fields["_preload_skills_thread"]
        fields["_preload_skills_thread"] = ThreadDouble() if isinstance(v, dict) else v
    if "_preload_skills_error" in fields:
        v = fields["_preload_skills_error"]
        if isinstance(v, dict) and "__raise__" in v:
            exc = make_exc(v["__raise__"], v.get("msg", ""))
            CUR["raised"].append((exc, v))
            fields["_preload_skills_error"] = exc
    for k in ("_auto_load_skills_result", "_preload_skills_result"):
        if isinstance(fields.get(k), list):
            fields[k] = tuple(fields[k])
    slf = types.SimpleNamespace(**fields)
    for nm in METHOD_TARGETS:
        setattr(slf, nm, types.MethodType(ns[nm], slf))
    slf.process_command = mk("process_command",
                             lambda a, k: {"command": a[0], "redispatch": k.get("redispatch")})
    slf._console_print = mk("console_print", lambda a, k: {"text": a[0] if a else ""})
    return slf


def echo_keys(slf, keys):
    return {k: getattr(slf, k) for k in keys if hasattr(slf, k)}


CB_KEYS = ["_tool_callbacks_installed"]
FIN_KEYS = ["_preload_skills_finalized", "preloaded_skills", "system_prompt"]
PI_KEYS = ["_pending_input"]


def call_echo(fn, echo):
    try:
        ret = fn()
    except BaseException:
        CUR["state"] = echo()
        raise
    CUR["state"] = echo()
    return ret, CUR["state"]


def drive(ns, func, inp):
    env = inp.get("env") or {}
    hcli = sys.modules["hermes_cli"]
    if env.get("security_import_ok", True):
        sys.modules["hermes_cli.security_advisories"] = SA_MOD
        hcli.security_advisories = SA_MOD
    else:
        sys.modules["hermes_cli.security_advisories"] = None
        hcli.security_advisories = None
    ns["sys"] = types.SimpleNamespace(stdin=TTYDouble("stdin.isatty"),
                                      stdout=TTYDouble("stdout.isatty"),
                                      stderr=sys.stderr)
    CUR["state"] = None

    if func == "configure_output_history":
        setup_module_state(ns, inp)
        echo = lambda: {"enabled": ns["_OUTPUT_HISTORY_ENABLED"],
                        "max_lines": ns["_OUTPUT_HISTORY_MAX_LINES"],
                        "history": hist_echo(ns)}
        return call_echo(lambda: ns["_configure_output_history"](
            inp.get("enabled"), inp.get("max_lines", 200)), echo)
    if func == "suspend_output_history":
        setup_module_state(ns, inp)
        during = [None]

        def body():
            with ns["_suspend_output_history"]():
                during[0] = ns["_OUTPUT_HISTORY_SUPPRESSED"]
                return seam("suspend_body", {})

        echo = lambda: {"suppressed_during": during[0],
                        "suppressed_after": ns["_OUTPUT_HISTORY_SUPPRESSED"]}
        return call_echo(body, echo)
    if func == "replay_output_history":
        setup_module_state(ns, inp)
        fit = inp.get("fit")
        if isinstance(fit, list):
            fit = tuple(fit)
        output = None
        if isinstance(inp.get("output"), dict):
            o = inp["output"]
            output = OutputDouble(o.get("rows"), o.get("columns"))
        echo = lambda: {"enabled": ns["_OUTPUT_HISTORY_ENABLED"],
                        "replaying": ns["_OUTPUT_HISTORY_REPLAYING"],
                        "history": hist_echo(ns)}
        return call_echo(lambda: ns["_replay_output_history"](fit=fit, output=output), echo)
    if func == "looks_like_slash_command":
        return ns["_looks_like_slash_command"](inp.get("text")), None
    if func == "slash_args":
        return ns["_slash_args"](inp.get("cmd")), None
    if func == "ensure_skill_commands":
        setup_module_state(ns, inp)
        echo = lambda: {"skill_commands": ns["_skill_commands"]}
        return call_echo(lambda: ns["_ensure_skill_commands"](), echo)
    if func == "session_skill_commands":
        setup_module_state(ns, inp)
        slf = mk_self(ns, inp)
        echo = lambda: {"skill_commands": ns["_skill_commands"]}
        return call_echo(lambda: ns["_session_skill_commands"](slf), echo)
    if func == "should_seed_interactive":
        return ns["_should_seed_interactive"](inp.get("query"), inp.get("image"),
                                              inp.get("quiet", False), inp.get("oneshot", False)), None
    if func == "voice_input_message_str":
        obj = ns["_VoiceInputMessage"](**(inp.get("self") or {}))
        return str(obj), None
    if func == "seeded_query_message_str":
        obj = ns["_SeededQueryMessage"](**(inp.get("self") or {}))
        return str(obj), None
    if func == "install_tool_callbacks":
        slf = mk_self(ns, inp)
        return call_echo(lambda: ns["_install_tool_callbacks"](slf), lambda: echo_keys(slf, CB_KEYS))
    if func == "show_security_advisories":
        slf = mk_self(ns, inp)
        return ns["_show_security_advisories"](slf), None
    if func == "finalize_preloaded_skills":
        slf = mk_self(ns, inp)
        return call_echo(lambda: ns["finalize_preloaded_skills"](slf), lambda: echo_keys(slf, FIN_KEYS))
    if func == "slash_handler":
        dispatch = {k: tuple(v) for k, v in (inp.get("dispatch") or {}).items()}
        cls = types.SimpleNamespace(_SLASH_DISPATCH=dispatch)
        for h in (inp.get("handlers") or []):
            setattr(cls, h, lambda *a, **k: None)
        return ns["_slash_handler"](cls, inp.get("canonical")), None
    if func == "process_unregistered_slash":
        slf = mk_self(ns, inp)
        return call_echo(lambda: ns["_process_unregistered_slash"](
            slf, inp.get("cmd_original"), inp.get("cmd_lower")), lambda: echo_keys(slf, PI_KEYS))
    if func == "run_quick_command":
        slf = mk_self(ns, inp)
        return ns["_run_quick_command"](slf, inp.get("base_cmd"), inp.get("qcmd"),
                                        inp.get("user_args", "")), None
    if func == "run_plugin_slash_command":
        slf = mk_self(ns, inp)
        return ns["_run_plugin_slash_command"](slf, inp.get("base_cmd"), inp.get("user_args", "")), None
    if func == "queue_skill_message":
        slf = mk_self(ns, inp)
        return call_echo(lambda: ns["_queue_skill_message"](slf, inp.get("msg")),
                         lambda: echo_keys(slf, PI_KEYS))
    if func == "run_skill_bundle_command":
        slf = mk_self(ns, inp)
        return call_echo(lambda: ns["_run_skill_bundle_command"](
            slf, inp.get("base_cmd"), inp.get("bundle_info"), inp.get("user_instruction", "")),
            lambda: echo_keys(slf, PI_KEYS))
    if func == "queue_loaded_skills":
        slf = mk_self(ns, inp)
        return call_echo(lambda: ns["_queue_loaded_skills"](slf, inp.get("msg"),
                                                            inp.get("label"), inp.get("missing")),
                         lambda: echo_keys(slf, PI_KEYS))
    if func == "run_skill_slash_command":
        slf = mk_self(ns, inp)
        return call_echo(lambda: ns["_run_skill_slash_command"](
            slf, inp.get("base_cmd"), inp.get("skill_info"), inp.get("rest", "")),
            lambda: echo_keys(slf, PI_KEYS))
    if func == "expand_slash_prefix":
        slf = mk_self(ns, inp)
        return ns["_expand_slash_prefix"](slf, inp.get("cmd_original"), inp.get("cmd_lower"),
                                          inp.get("skill_commands"), inp.get("skill_bundles")), None
    raise ValueError("unknown func: %s" % func)


def main():
    ns = extract()
    install_module_stubs()
    cases = 0
    mism = 0
    per_func = {}
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
        per_func[func] = per_func.get(func, 0) + 1
        CUR["trace"] = []
        CUR["scripts"] = inp.get("seams", {})
        CUR["idx"] = {}
        CUR["state"] = None
        CUR["raised"] = []
        CUR["intern"] = {}
        if func not in PY:
            mism += 1
            print("MISMATCH %s: unknown func (not in PY map)" % func)
            continue
        try:
            ret, state = drive(ns, func, inp)
        except BaseException as ex:
            spec = None
            for (e, s) in CUR["raised"]:
                if e is ex:
                    spec = s
                    break
            ret = spec if spec is not None else {"__raise__": type(ex).__name__, "msg": str(ex)}
            state = CUR["state"]
        want = {"ret": norm(ret), "trace": norm(CUR["trace"])}
        if state is not None:
            want["state"] = norm(state)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE cli_py_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE cli_py_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()