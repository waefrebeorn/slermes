#!/usr/bin/env python3
"""Oracle for setup_flow2.c — the UNCLAIMED hermes_cli/setup.py FLOW cores +
leaves (LIVE authority): the print/info/banner emit leaves, the env-var
prompt-and-save leaf, the module-probe leaf, the number/int-setting prompt
leaves, the consent-completion leaf, the wizard orchestrators (run_setup_wizard,
_run_setup_wizard_impl, _run_setup_section, _run_full_setup), the navigation
state ctors/reset and the wizard's nested closures (_record_answers, _skip,
_gateway_step).

Drives the LIVE defs with SCRIPTED I/O: defs are AST-extracted from the quarry
(no heavy hermes_cli import), module constants seeded (Assign AND AnnAssign,
twice — SETUP_SECTIONS captures the stable seam recorders), and every claimed
sibling (prompt, prompt_choice, print_header, is_interactive_stdin,
print_noninteractive_setup_guidance, _apply_default_agent_settings,
_run_setup_steps, _setup_navigation_scope, _skip_configured_section,
setup_gateway, the SETUP_SECTIONS functions) plus every foreign collaborator
(config helpers, cli_output, colors.color, importlib.util.find_spec, the
observability writers, backup_config, get_active_provider, offer_agent_import,
the setup_quick runners, _print_setup_summary, ensure_gateway_service) replaced
by recording stubs returning the SAME scripted results the C harness feeds its
universal seam. Ported-to-ported calls compose REAL on both sides (st2_info /
st2_print_banner / st2_prompt_number / st2_run_setup_wizard_impl / ...), so
their internal print/print_info/color events appear in both traces.

The _setup_navigation_scope context manager is a ScopeCM double recording
setup_navigation_scope.enter/.exit (exit sees the in-flight exception; an exit
raise replaces it; exit never suppresses — script null or a raise only).
backup_config results materialize as BackupPath doubles whose .exists() fires
the backup_path.exists seam. Path-valued seam results materialize as
PurePosixPath so str() keeps POSIX separators on this Windows host.

Fail-closed: a missing/empty seam script RETURNS a RuntimeError raise spec
(recorded as the event ret, then raised), matching the C harness. Uncaught
raises (incl. the BaseException trio and the real _SetupControlFlow family)
are caught into a ret spec; `except ValueError` / `except Exception` /
`except _SetupCancelled` sites swallow exactly their live kinds.

Input: stdin JSON lines from t_port_setup_flow2:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...], "state":{...}}}
Summary: ORACLE setup_flow2: N cases, M mismatches. Fail-closed (exit 2) when
extraction misses defs or zero cases were compared.
"""
import ast
import builtins
import copy
import json
import logging
import os
import re
import sys
import types
import __future__
from contextlib import contextmanager
from contextvars import ContextVar
from pathlib import Path, PurePath, PurePosixPath
from typing import Callable

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not QUARRY.is_dir():
    print("ORACLE setup_flow2: quarry not found at %s" % QUARRY, file=sys.stderr)
    sys.exit(2)
# The runner scrubs the environment; give the quarry chain a home.
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "setup_flow2_home"))

SRC = str(QUARRY / "hermes_cli" / "setup.py")
SRC_TEXT = open(SRC, encoding="utf-8").read()
ANN_FLAG = __future__.annotations.compiler_flag

# harness dispatch name -> live def name
PY = {
    "info": "_info",
    "sub_dict": "_sub_dict",
    "section_rule": "_section_rule",
    "print_banner": "_print_banner",
    "module_installed": "_module_installed",
    "prompt_number": "_prompt_number",
    "prompt_int_setting": "_prompt_int_setting",
    "prompt_and_save_env_var": "_prompt_and_save_env_var",
    "record_setup_completed": "_record_setup_completed",
    "run_setup_wizard": "run_setup_wizard",
    "run_setup_section": "_run_setup_section",
    "run_full_setup": "_run_full_setup",
    "run_setup_wizard_impl": "_run_setup_wizard_impl",
    "nav_state_init": "_SetupNavigationState.__init__",
    "nav_state_reset": "_SetupNavigationState.reset",
    "go_back_init": "_SetupGoBack.__init__",
    "record_answers": "_record_answers",
    "skip": "_skip",
    "gateway_step": "_gateway_step",
}
TOP_TARGETS = [
    "_info", "_sub_dict", "_section_rule", "_prompt_and_save_env_var",
    "_module_installed", "_print_banner", "_prompt_number", "_prompt_int_setting",
    "run_setup_wizard", "_run_setup_section", "_run_full_setup",
    "_run_setup_wizard_impl", "_record_setup_completed",
]
CLASSES = ["_SetupControlFlow", "_SetupCancelled", "_SetupGoBack", "_SetupNavigationState"]
# nested closure -> (parent def, free-var globals it needs at drive time)
NESTED = {
    "_record_answers": "_run_setup_steps",
    "_skip": "_run_full_setup",
    "_gateway_step": "_run_full_setup",
}

CUR = {"scripts": {}, "idx": {}, "trace": [], "state": None}
_UNSET = object()
EXC_SETUP = {}   # real _SetupControlFlow family classes (filled at extract)


class RecLogger:
    def debug(self, *a, **k): pass
    def info(self, *a, **k): pass
    def warning(self, *a, **k): pass
    def error(self, *a, **k): pass


BASE_EXC = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
BUILTIN_EXC = {
    "RuntimeError": RuntimeError, "ValueError": ValueError, "TypeError": TypeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "IndexError": IndexError,
    "Exception": Exception, "StopIteration": StopIteration, "OSError": OSError,
    "EOFError": EOFError, "ImportError": ImportError, "UnicodeDecodeError": UnicodeDecodeError,
}


def make_exc(kind, msg):
    cls = BASE_EXC.get(kind) or EXC_SETUP.get(kind) or BUILTIN_EXC.get(kind)
    if cls is None:
        cls = type(str(kind), (Exception,), {})
    return cls(msg)


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r["__raise__"], r.get("msg", ""))
    return r


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    scripts = CUR["scripts"]
    # Distinguish a MISSING script (fail-closed) from a scripted null (None).
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
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


def norm(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, PurePath):
        return o.as_posix()
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, (set, frozenset)):
        return sorted(norm(v) for v in o)
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


# ── stable recording stubs (read CUR at call time; installed once) ──

def mk(name, proj):
    def f(*a, **k):
        return seam(name, proj(a, k))
    f.__name__ = name
    return f


def _cfg_proj(a, k):
    return {"config": copy.deepcopy(a[0])}


def prompt_rec(question, default=None, password=_UNSET):
    a = {"question": question}
    if default is not None:
        a["default"] = default
    if password is not _UNSET:
        a["password"] = password
    return seam("prompt", a)


def prompt_choice_rec(question, choices, default=0, description=None):
    a = {"question": question, "choices": choices, "default": default}
    if description is not None:
        a["description"] = description
    return seam("prompt_choice", a)


def print_header_rec(title, *, gap=False):
    return seam("print_header", {"title": title, "gap": gap})


def print_rec(*a, **k):
    return seam("print", {"args": list(a)})


def color_rec(text, *codes):
    return seam("color", {"text": text, "codes": list(codes)})


def path_rec(name):
    def f(*a, **k):
        r = seam(name, {})
        return PurePosixPath(r) if isinstance(r, str) else r
    f.__name__ = name
    return f


class BackupPath:
    """backup_config result double: str() is the POSIX path; .exists() seamed."""
    def __init__(self, path):
        self.path = path

    def __str__(self):
        return self.path

    def exists(self):
        return seam("backup_path.exists", {})


def backup_config_rec(config_path, label):
    r = seam("backup_config", {"config_path": config_path, "label": label})
    if isinstance(r, dict) and "path" in r:
        return BackupPath(r["path"])
    return r


class ScopeCM:
    """_setup_navigation_scope double: enter/exit seam pair, never suppresses."""
    def __enter__(self):
        return seam("setup_navigation_scope.enter", {})

    def __exit__(self, et, ev, tb):
        exc = {"kind": et.__name__, "msg": str(ev)} if et is not None else None
        seam("setup_navigation_scope.exit", {"exc": exc})
        return False


def proj_action(fn):
    """Project a steps-list closure to the C action name (see port header)."""
    nm = getattr(fn, "__name__", "")
    if nm == "_gateway_step":
        return "gateway_step"
    code = getattr(fn, "__code__", None)
    fv = {}
    if code is not None and getattr(fn, "__closure__", None):
        fv = dict(zip(code.co_freevars, (c.cell_contents for c in fn.__closure__)))
    if "key" in fv and "run" in fv:
        return "step:" + fv["key"]
    if "runner" in fv:
        return "first_time:" + fv["runner"]
    if "func" in fv:
        return getattr(fv["func"], "__name__", "?")
    if code is not None and "_run_quick_setup" in code.co_names:
        return "run_quick_setup"
    return nm


def run_setup_steps_rec(steps):
    return seam("run_setup_steps",
                {"steps": [[lab, proj_action(act)] for lab, act in steps]})


def record_setup_completed_rec(*a, **k):
    return seam("record_setup_completed",
                {"surface": k.get("surface"), "provider": k.get("provider")})


def ensure_gateway_service_rec(*a, **k):
    return seam("ensure_gateway_service",
                {"context": k.get("context"), "interactive": k.get("interactive"),
                 "config": copy.deepcopy(k.get("config"))})


# names bound into the exec namespace (claimed siblings + foreign collaborators)
def seam_bindings():
    b = {
        "print": print_rec,
        "print_info": mk("print_info", lambda a, k: {"msg": a[0]}),
        "print_success": mk("print_success", lambda a, k: {"msg": a[0]}),
        "print_warning": mk("print_warning", lambda a, k: {"msg": a[0]}),
        "print_error": mk("print_error", lambda a, k: {"msg": a[0]}),
        "color": color_rec,
        "prompt": prompt_rec,
        "prompt_choice": prompt_choice_rec,
        "print_header": print_header_rec,
        "is_interactive_stdin": mk("is_interactive_stdin", lambda a, k: {}),
        "print_noninteractive_setup_guidance": mk(
            "print_noninteractive_setup_guidance",
            lambda a, k: {"reason": a[0] if a else k.get("reason")}),
        "_apply_default_agent_settings": mk("_apply_default_agent_settings", _cfg_proj),
        "_run_setup_steps": run_setup_steps_rec,
        "_setup_navigation_scope": lambda: ScopeCM(),
        "_skip_configured_section": mk(
            "_skip_configured_section",
            lambda a, k: {"config": copy.deepcopy(a[0]), "key": a[1], "label": a[2]}),
        "load_config": mk("load_config", lambda a, k: {}),
        "save_config": mk("save_config", _cfg_proj),
        "get_config_path": path_rec("get_config_path"),
        "get_env_path": path_rec("get_env_path"),
        "get_hermes_home": path_rec("get_hermes_home"),
        "get_env_value": mk("get_env_value", lambda a, k: {"name": a[0]}),
        "ensure_hermes_home": mk("ensure_hermes_home", lambda a, k: {}),
        "save_env_value": mk("save_env_value", lambda a, k: {"name": a[0], "value": a[1]}),
        "offer_agent_import": mk("offer_agent_import", lambda a, k: {"hermes_home": a[0]}),
        "_run_portal_one_shot": mk("run_portal_one_shot", _cfg_proj),
        "_run_quick_setup": mk("_run_quick_setup",
                               lambda a, k: {"config": copy.deepcopy(a[0]), "hermes_home": a[1]}),
        "_print_setup_summary": mk("_print_setup_summary",
                                   lambda a, k: {"config": copy.deepcopy(a[0]), "hermes_home": a[1]}),
        "setup_model_provider": mk("setup_model_provider", _cfg_proj),
        "setup_tts": mk("setup_tts", _cfg_proj),
        "setup_terminal_backend": mk("setup_terminal_backend", _cfg_proj),
        "setup_gateway": mk("setup_gateway", _cfg_proj),
        "setup_tools": mk("setup_tools", _cfg_proj),
        "setup_telemetry": mk("setup_telemetry", _cfg_proj),
        "setup_agent_settings": mk("setup_agent_settings", _cfg_proj),
    }
    return b


def install_module_stubs():
    """sys.modules stubs for every in-function import the targets execute."""
    hcli = types.ModuleType("hermes_cli")
    hcli.__path__ = []
    sys.modules["hermes_cli"] = hcli

    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    cfg = mod("hermes_cli.config",
              is_managed=mk("is_managed", lambda a, k: {}),
              managed_error=mk("managed_error", lambda a, k: {"msg": a[0]}))
    hcli.config = cfg
    mod("hermes_cli.config_backups", backup_config=backup_config_rec)
    mod("hermes_cli.auth", get_active_provider=mk("get_active_provider", lambda a, k: {}))
    obs = mod("hermes_cli.observability")
    obs.__path__ = []
    hcli.observability = obs
    mod("hermes_cli.observability.shared_metrics_consent",
        offer_consent_if_undecided=mk("offer_consent_if_undecided", _cfg_proj))
    mod("hermes_cli.observability.shared_metrics_events",
        record_setup_completed=record_setup_completed_rec)
    mod("hermes_cli.gateway_setup_service",
        ensure_gateway_service=ensure_gateway_service_rec)
    sq = mod("hermes_cli.setup_quick",
             _run_first_time_quick_setup=mk("_run_first_time_quick_setup", lambda a, k: {}),
             _run_blank_slate_setup=mk("_run_blank_slate_setup", lambda a, k: {}),
             _run_quick_setup=mk("_run_quick_setup", lambda a, k: {}),
             _run_portal_one_shot=mk("run_portal_one_shot", lambda a, k: {}))
    hcli.setup_quick = sq


# ── extraction ──

def extract():
    ns = {
        "__name__": "hermes_cli.setup_flow2_extracted",
        "__builtins__": builtins.__dict__,
        "copy": copy, "re": re, "os": os, "sys": sys, "logging": logging,
        "Path": Path, "Callable": Callable, "contextmanager": contextmanager,
        "ContextVar": ContextVar,
        "importlib": types.SimpleNamespace(
            util=types.SimpleNamespace(find_spec=mk("find_spec", lambda a, k: {"name": a[0]}))),
        "project_root_str": lambda: "/quarry",
        "logger": RecLogger(),
    }
    ns.update(seam_bindings())
    # Colors class from the quarry colors module (real ANSI constants).
    csrc = (QUARRY / "hermes_cli" / "colors.py").read_text(encoding="utf-8")
    for node in ast.parse(csrc).body:
        if isinstance(node, ast.ClassDef) and node.name == "Colors":
            exec(compile(ast.Module(body=[node], type_ignores=[]), "colors.py", "exec"), ns)

    tree = ast.parse(SRC_TEXT)

    def exec_node(node):
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                     flags=ANN_FLAG), ns)

    def seed_consts():
        for node in tree.body:
            if isinstance(node, ast.Assign):
                if not all(isinstance(t, ast.Name) for t in node.targets):
                    continue
            elif isinstance(node, ast.AnnAssign):
                if not isinstance(node.target, ast.Name):
                    continue
            else:
                continue
            try:
                exec_node(node)
            except Exception:
                pass

    seed_consts()   # SETUP_SECTIONS captures the stable seam recorders
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in CLASSES:
            exec_node(node)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in TOP_TARGETS:
            exec_node(node)
    seed_consts()   # constants that reference functions
    ns["logger"] = RecLogger()   # logger = logging.getLogger(...) is an Assign

    missing = [n for n in TOP_TARGETS + CLASSES if n not in ns]
    if missing:
        print("ORACLE setup_flow2: FATAL missing defs: %s" % sorted(missing), file=sys.stderr)
        sys.exit(2)
    EXC_SETUP.update({n: ns[n] for n in ("_SetupControlFlow", "_SetupCancelled", "_SetupGoBack")})
    return ns


def extract_nested(parent_name, nested_name, globals_dict):
    """Exec a nested closure def with its free variables bound as globals."""
    tree = ast.parse(SRC_TEXT)
    parent = next(n for n in tree.body
                  if isinstance(n, ast.FunctionDef) and n.name == parent_name)
    node = next(n for n in parent.body
                if isinstance(n, ast.FunctionDef) and n.name == nested_name)
    ns = {"__builtins__": builtins.__dict__, "copy": copy}
    ns.update(globals_dict)
    exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                 flags=ANN_FLAG), ns)
    return ns[nested_name]


# ── drive ──

def nav_fields(obj):
    return {
        "section_index": obj.section_index,
        "prompt_index": obj.prompt_index,
        "active_prompt_index": obj.active_prompt_index,
        "resolved_choices": obj.resolved_choices,
        "replay_choices": obj.replay_choices,
    }


def nav_from(ns, d):
    obj = ns["_SetupNavigationState"]()
    for k, v in (d or {}).items():
        setattr(obj, k, copy.deepcopy(v))
    return obj


def drive(ns, func, inp):
    env = inp.get("env") or {}
    ns["PROJECT_ROOT"] = PurePosixPath(env.get("PROJECT_ROOT", "/quarry"))
    ns["DEFAULT_CONFIG"] = copy.deepcopy(env.get("DEFAULT_CONFIG") or {})
    CUR["state"] = None

    if func == "info":
        return ns["_info"](*(inp.get("lines") or [])), None
    if func == "sub_dict":
        parent = copy.deepcopy(inp.get("parent") or {})
        CUR["state"] = {"parent": parent}
        ret = ns["_sub_dict"](parent, inp.get("key", ""))
        return ret, {"parent": parent}
    if func == "section_rule":
        ns["_section_rule"](inp.get("title", ""))
        return None, None
    if func == "print_banner":
        ns["_print_banner"](*(inp.get("lines") or []))
        return None, None
    if func == "module_installed":
        return ns["_module_installed"](inp.get("name", "")), None
    if func == "prompt_number":
        cast = float if inp.get("cast", "int") == "float" else int
        return ns["_prompt_number"](inp.get("label", ""), inp.get("current"), cast), None
    if func == "prompt_int_setting":
        section = copy.deepcopy(inp.get("section") or {})
        CUR["state"] = {"section": section}
        accept = lambda v: seam("accept", {"value": v})
        ns["_prompt_int_setting"](section, inp.get("key", ""), inp.get("label", ""),
                                  inp.get("current"), accept)
        return None, {"section": section}
    if func == "prompt_and_save_env_var":
        var = copy.deepcopy(inp.get("var") or {})
        ns["_prompt_and_save_env_var"](var, inp.get("saved_msg", ""), inp.get("skipped_msg", ""))
        return None, None
    if func == "record_setup_completed":
        config = copy.deepcopy(inp.get("config") or {})
        CUR["state"] = {"config": config}
        ns["_record_setup_completed"](config)
        return None, {"config": config}
    if func == "run_setup_wizard":
        args = types.SimpleNamespace(**(inp.get("args") or {}))
        return ns["run_setup_wizard"](args), None
    if func == "run_setup_section":
        config = copy.deepcopy(inp.get("config") or {})
        CUR["state"] = {"config": config}
        ns["_run_setup_section"](config, inp.get("section"))
        return None, {"config": config}
    if func == "run_full_setup":
        config = copy.deepcopy(inp.get("config") or {})
        CUR["state"] = {"config": config}
        home = PurePosixPath(inp.get("hermes_home", ""))
        ns["_run_full_setup"](config, home,
                              is_existing=inp.get("is_existing", False),
                              imported_from=inp.get("imported_from", ""))
        return None, {"config": config}
    if func == "run_setup_wizard_impl":
        args = types.SimpleNamespace(**(inp.get("args") or {}))
        return ns["_run_setup_wizard_impl"](args), None
    if func == "nav_state_init":
        obj = ns["_SetupNavigationState"](section_index=inp.get("section_index", -1),
                                          prompt_index=inp.get("prompt_index", 0))
        return None, {"nav": nav_fields(obj)}
    if func == "nav_state_reset":
        obj = nav_from(ns, inp.get("nav"))
        si = inp.get("section_index", -1)
        if "replay" in inp:
            obj.reset(si, inp["replay"])
        else:
            obj.reset(si)
        return None, {"nav": nav_fields(obj)}
    if func == "go_back_init":
        e = ns["_SetupGoBack"](inp.get("prompt_index", 0))
        return {"prompt_index": e.prompt_index, "args": list(e.args)}, None
    if func == "record_answers":
        nj = inp.get("nav")
        state = nav_from(ns, nj) if isinstance(nj, dict) else None
        answers = copy.deepcopy(inp.get("answers") or {})
        si = inp.get("section_index", 0)
        f = extract_nested("_run_setup_steps", "_record_answers",
                           {"state": state, "section_index": si,
                            "answers_by_section": answers})
        f()
        return None, {"answers": answers, "nav": nav_fields(state) if state is not None else None}
    if func == "skip":
        config = copy.deepcopy(inp.get("config") or {})
        imp = inp.get("imported_from", "")
        f = extract_nested("_run_full_setup", "_skip",
                           {"config": config, "imported_from": imp,
                            "_skip_configured_section": ns["_skip_configured_section"]})
        return f(inp.get("key", ""), inp.get("label", "")), None
    if func == "gateway_step":
        config = copy.deepcopy(inp.get("config") or {})
        CUR["state"] = {"config": config}
        imp = inp.get("imported_from", "")
        skipf = extract_nested("_run_full_setup", "_skip",
                               {"config": config, "imported_from": imp,
                                "_skip_configured_section": ns["_skip_configured_section"]})
        f = extract_nested("_run_full_setup", "_gateway_step",
                           {"config": config, "_skip": skipf,
                            "setup_gateway": ns["setup_gateway"]})
        f()
        return None, {"config": config}
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
        if func not in PY:
            mism += 1
            print("MISMATCH %s: unknown func (not in PY map)" % func)
            continue
        try:
            ret, state = drive(ns, func, inp)
        except BaseException as ex:
            ret = {"__raise__": type(ex).__name__, "msg": str(ex)}
            state = CUR["state"]
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if state is not None:
            want["state"] = norm(state)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE setup_flow2: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE setup_flow2: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()