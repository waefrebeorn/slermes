#!/usr/bin/env python3
"""Oracle for commands_flow.c — the command-surface FLOW cores + leaves of
hermes_cli/commands.py and its derivation siblings (commands_platforms.py /
commands_completion.py / slash_exec.py / cli_unknown_command.py).

Drives the LIVE functions with SCRIPTED I/O: defs are AST-extracted from the
quarry (no heavy hermes_cli import), the needed module constants seeded
(_CATEGORY_SLUGS / _TG_DASHES), CommandDef / CommandContext / CommandReply
exec'd as real dataclasses, and every foreign effect (agent.i18n.t, the claimed
siblings _build_description / _resolve_config_gates / _sanitize_telegram_name /
_file_size_label, the skill/plugin/config seams, os.listdir/isdir, difflib.
get_close_matches, Path.resolve, the logger.warning fallback and the text_for /
provider callbacks) replaced by recording stubs returning the SAME scripted
results the C harness feeds its universal seam. COMMAND_REGISTRY is injected
per case from the fixture "registry" (CommandDef field dicts). Compares the
ordered DECISION SEQUENCE + return.

Fail-closed: a missing/empty seam script returns a RuntimeError raise spec
(recorded as the event ret, then raised), matching the C harness. Uncaught
raises (incl. the BaseException trio) are caught into a ret spec; `except
Exception` sites in live swallow plain Exceptions but pass BaseException
through. Import gates (fixture "import_gates") set sys.modules[mod] = None so
the live from-import raises ModuleNotFoundError exactly where the C port parks
its halt spec.

Input: stdin JSON lines from t_port_commands_flow:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...]}}
Summary: ORACLE commands_flow: N cases, M mismatches. Fail-closed (exit 2)
when extraction misses defs or zero cases were compared.
"""
import ast
import json
import os
import posixpath
import re
import sys
import types
from dataclasses import asdict, dataclass, field, is_dataclass
from pathlib import Path, PurePath, PurePosixPath

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC_COMMANDS = os.path.join(QUARRY, "hermes_cli", "commands.py")
SRC_PLATFORMS = os.path.join(QUARRY, "hermes_cli", "commands_platforms.py")
SRC_COMPLETION = os.path.join(QUARRY, "hermes_cli", "commands_completion.py")
SRC_SLASHEXEC = os.path.join(QUARRY, "hermes_cli", "slash_exec.py")
SRC_UNKNOWN = os.path.join(QUARRY, "hermes_cli", "cli_unknown_command.py")

# harness func name -> live python name
PY = {
    "localized": "_localized",
    "category_label": "category_label",
    "describe": "describe",
    "desktop_surface_registry": "desktop_surface_registry",
    "build_commands_by_category": "build_commands_by_category",
    "build_commands": "build_commands",
    "is_gateway_available": "_is_gateway_available",
    "module_getattr": "__getattr__",
    "gateway_available_commands": "_gateway_available_commands",
    "normalize_telegram_desc": "_normalize_telegram_desc",
    "truncate_desc": "_truncate_desc",
    "sanitized_rank": "_sanitized_rank",
    "iter_gateway_skills": "_iter_gateway_skills",
    "gateway_bundles": "_gateway_bundles",
    "completion": "_completion",
    "prefix_completions": "_prefix_completions",
    "split_args": "_split_args",
    "dir_completions": "_dir_completions",
    "call_provider": "_call_provider",
    "exec_autoprune": "_exec_autoprune",
    "skill_commands": "_skill_commands",
    "unknown_command_lines": "unknown_command_lines",
}
WANTED = set(PY.values())
# (source, class-or-None) for each wanted def
DEF_HOME = {
    "_localized": (SRC_COMMANDS, None),
    "category_label": (SRC_COMMANDS, None),
    "describe": (SRC_COMMANDS, "CommandDef"),
    "desktop_surface_registry": (SRC_COMMANDS, None),
    "build_commands_by_category": (SRC_COMMANDS, None),
    "build_commands": (SRC_COMMANDS, None),
    "_is_gateway_available": (SRC_COMMANDS, None),
    "__getattr__": (SRC_COMMANDS, None),
    "_gateway_available_commands": (SRC_PLATFORMS, None),
    "_normalize_telegram_desc": (SRC_PLATFORMS, None),
    "_truncate_desc": (SRC_PLATFORMS, None),
    "_sanitized_rank": (SRC_PLATFORMS, None),
    "_iter_gateway_skills": (SRC_PLATFORMS, None),
    "_gateway_bundles": (SRC_PLATFORMS, None),
    "_completion": (SRC_COMPLETION, None),
    "_prefix_completions": (SRC_COMPLETION, None),
    "_split_args": (SRC_COMPLETION, None),
    "_dir_completions": (SRC_COMPLETION, None),
    "_call_provider": (SRC_COMPLETION, "SlashCommandCompleter"),
    "_exec_autoprune": (SRC_SLASHEXEC, None),
    "_skill_commands": (SRC_SLASHEXEC, None),
    "unknown_command_lines": (SRC_UNKNOWN, None),
}
# module constants the ported defs read (selective seeding; the heavy
# COMMAND_REGISTRY / SUBCOMMANDS / EXECUTORS tables are injected or unused)
CONSTS = {
    SRC_COMMANDS: ["_CATEGORY_SLUGS"],
    SRC_PLATFORMS: ["_TG_DASHES"],
}
CLASSES = {
    SRC_COMMANDS: ["CommandDef"],
    SRC_SLASHEXEC: ["CommandContext", "CommandReply"],
}

CUR = {"trace": [], "scripts": {}, "idx": {}}

BASE_EXC = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
BUILTIN_EXC = {
    "RuntimeError": RuntimeError, "ValueError": ValueError, "TypeError": TypeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "NameError": NameError,
    "LookupError": LookupError, "IndexError": IndexError, "Exception": Exception,
    "StopIteration": StopIteration, "OSError": OSError,
    "FileNotFoundError": FileNotFoundError, "PermissionError": PermissionError,
    "NotADirectoryError": NotADirectoryError, "IsADirectoryError": IsADirectoryError,
    "FileExistsError": FileExistsError, "TimeoutError": TimeoutError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "AssertionError": AssertionError, "ArithmeticError": ArithmeticError,
    "ZeroDivisionError": ZeroDivisionError, "OverflowError": OverflowError,
    "UnicodeError": UnicodeError, "UnicodeDecodeError": UnicodeDecodeError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "MemoryError": MemoryError, "BufferError": BufferError, "EOFError": EOFError,
    "ReferenceError": ReferenceError, "SystemError": SystemError,
    "SyntaxError": SyntaxError, "IndentationError": IndentationError,
    "TabError": TabError, "UnboundLocalError": UnboundLocalError,
    "FloatingPointError": FloatingPointError, "StopAsyncIteration": StopAsyncIteration,
    "Warning": Warning, "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "ResourceWarning": ResourceWarning, "EncodingWarning": EncodingWarning,
    "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError,
    "BlockingIOError": BlockingIOError, "ChildProcessError": ChildProcessError,
    "ProcessLookupError": ProcessLookupError, "InterruptedError": InterruptedError,
    "JSONDecodeError": json.JSONDecodeError,
}


def make_exc(kind, msg):
    cls = BASE_EXC.get(kind) or BUILTIN_EXC.get(kind)
    if cls is None:
        cls = type(str(kind), (Exception,), {})
    if cls is json.JSONDecodeError:
        return cls(msg, "", 0)
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


# ── record doubles ──

class CompletionDouble:
    """prompt_toolkit Completion projection (the 4 fields _completion passes)."""
    def __init__(self, text, start_position=0, display=None, display_meta=None):
        self.text = text
        self.start_position = start_position
        self.display = display
        self.display_meta = display_meta


class PathDouble(PurePosixPath):
    """Lexical POSIX Path; .resolve() is the path_resolve seam (no filesystem)."""
    def resolve(self, strict=False):
        return PathDouble(seam("path_resolve", {"path": str(self)}))


class RecLogger:
    def debug(self, *a, **k): pass
    def info(self, *a, **k): pass
    def error(self, *a, **k): pass
    def warning(self, fmt, *a, **k):
        return seam("logger.warning", {"msg": fmt, **k})
    def log(self, *a, **k): pass


def norm(o):
    if o is None or isinstance(o, (bool, int, str)):
        return o
    if isinstance(o, float):
        return o
    if isinstance(o, BaseException):
        return str(o)
    if isinstance(o, PurePath):
        return o.as_posix()
    if isinstance(o, CompletionDouble):
        return {"text": norm(o.text), "start_position": norm(o.start_position),
                "display": norm(o.display), "display_meta": norm(o.display_meta)}
    if is_dataclass(o) and not isinstance(o, type):
        return norm(asdict(o))
    if isinstance(o, dict):
        return {str(k): norm(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [norm(v) for v in o]
    if isinstance(o, (set, frozenset)):
        return sorted(norm(v) for v in o)
    return str(o)


# ── extraction ──

NS = None


def extract():
    global NS
    mod = types.ModuleType("hermes_cli.commands")
    ns = mod.__dict__
    ns["__name__"] = "hermes_cli.commands"
    sys.modules["hermes_cli.commands"] = mod
    hcli = types.ModuleType("hermes_cli")
    hcli.__path__ = []
    hcli.commands = mod
    sys.modules["hermes_cli"] = hcli

    ns["re"] = re
    ns["json"] = json
    ns["dataclass"] = dataclass
    ns["field"] = field
    ns["asdict"] = asdict
    ns["PurePosixPath"] = PurePosixPath
    ns["Completion"] = CompletionDouble
    ns["logger"] = RecLogger()

    trees = {}
    for src in (SRC_COMMANDS, SRC_PLATFORMS, SRC_COMPLETION, SRC_SLASHEXEC, SRC_UNKNOWN):
        trees[src] = ast.parse(open(src, encoding="utf-8").read())

    # dataclass record classes FIRST (constants / materialization need them)
    for src, names in CLASSES.items():
        for node in trees[src].body:
            if isinstance(node, ast.ClassDef) and node.name in names:
                exec(compile(ast.Module(body=[node], type_ignores=[]), src, "exec"), ns)

    # selective module constants (plain-Name targets only)
    for src, names in CONSTS.items():
        for node in trees[src].body:
            tgt = None
            if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
                tgt = [t.id for t in node.targets]
            elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
                tgt = [node.target.id]
            if tgt and any(t in names for t in tgt):
                exec(compile(ast.Module(body=[node], type_ignores=[]), src, "exec"), ns)
    for names in CONSTS.values():
        missing = [n for n in names if n not in ns]
        if missing:
            print("FATAL missing constants: %s" % missing)
            sys.exit(2)

    # wanted defs (module-level or class methods, decorators stripped)
    for name, (src, cls) in DEF_HOME.items():
        body = trees[src].body
        if cls is not None:
            cdef = next(n for n in body if isinstance(n, ast.ClassDef) and n.name == cls)
            body = cdef.body
        node = next((n for n in body
                     if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name == name), None)
        if node is None:
            print("FATAL def not found: %s in %s (%s)" % (name, src, cls))
            sys.exit(2)
        node.decorator_list = []  # strip @staticmethod / @property wrappers
        node = ast.fix_missing_locations(ast.Module(body=[node], type_ignores=[]))
        exec(compile(node, src, "exec"), ns)
    # re-seed logger AFTER any Assign seeding (logger = getLogger is an Assign)
    ns["logger"] = RecLogger()
    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    NS = ns
    return ns


# ── recording stubs (stateless over CUR; installed once) ──

STUB_MODS = {}


def install_stubs(ns):
    ns["t"] = lambda key, lang=None, **kw: seam("t", {"key": key, **kw})
    ns["_build_description"] = lambda cmd: seam("_build_description", {"cmd": asdict(cmd)})
    ns["_resolve_config_gates"] = lambda: seam("_resolve_config_gates", {})
    ns["_sanitize_telegram_name"] = lambda raw: seam("_sanitize_telegram_name", {"raw": raw})
    ns["_file_size_label"] = lambda path: seam("_file_size_label", {"path": path})
    ns["_LAZY_LOCALIZED_TABLES"] = {
        "COMMANDS": lambda: seam("build_commands", {}),
        "COMMANDS_BY_CATEGORY": lambda: seam("build_commands_by_category", {}),
    }
    ns["os"] = types.SimpleNamespace(
        path=types.SimpleNamespace(
            dirname=posixpath.dirname, basename=posixpath.basename, join=posixpath.join,
            isdir=lambda p: seam("os_isdir", {"path": p}),
        ),
        listdir=lambda p: seam("os_listdir", {"path": p}),
    )

    def stub_module(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        STUB_MODS[name] = m
        return m

    agent = types.ModuleType("agent")
    agent.__path__ = []
    STUB_MODS["agent"] = agent
    tools = types.ModuleType("tools")
    tools.__path__ = []
    STUB_MODS["tools"] = tools
    stub_module("agent.skill_commands",
                get_skill_commands=lambda: seam("get_skill_commands", {}))
    stub_module("agent.skill_utils",
                get_disabled_skill_names=lambda platform=None: seam("get_disabled_skill_names", {"platform": platform}),
                get_external_skills_dirs=lambda: seam("get_external_skills_dirs", {}),
                get_project_skills_dirs=lambda: seam("get_project_skills_dirs", {}))
    stub_module("agent.skill_bundles",
                get_skill_bundles=lambda: seam("get_skill_bundles", {}))
    stub_module("tools.skills_tool",
                _skills_dir=lambda: PathDouble(seam("_skills_dir", {})))
    stub_module("hermes_cli.prune_advisor",
                run_command=lambda args: seam("run_command", {"args": args}))
    stub_module("difflib",
                get_close_matches=lambda word, possibilities, n=3, cutoff=0.6: seam(
                    "get_close_matches",
                    {"word": word, "possibilities": possibilities, "n": n, "cutoff": cutoff}))
    # difflib is a module-top import used as a BARE GLOBAL in unknown_command_lines
    ns["difflib"] = STUB_MODS["difflib"]
    pathlib_stub = types.ModuleType("pathlib")
    pathlib_stub.Path = PathDouble
    pathlib_stub.PurePosixPath = PurePosixPath
    STUB_MODS["pathlib"] = pathlib_stub
    # link parent -> child attributes
    agent.skill_commands = STUB_MODS["agent.skill_commands"]
    agent.skill_utils = STUB_MODS["agent.skill_utils"]
    agent.skill_bundles = STUB_MODS["agent.skill_bundles"]
    tools.skills_tool = STUB_MODS["tools.skills_tool"]
    sys.modules["hermes_cli"].prune_advisor = STUB_MODS["hermes_cli.prune_advisor"]
    for name, m in STUB_MODS.items():
        sys.modules[name] = m


GATED_CANDIDATES = ["agent.skill_commands", "agent.skill_utils", "agent.skill_bundles",
                    "tools.skills_tool", "hermes_cli.prune_advisor"]


def apply_import_gates(inp):
    gates = inp.get("import_gates") or {}
    for name in GATED_CANDIDATES:
        sys.modules[name] = None if gates.get(name) else STUB_MODS[name]


# ── drive ──

def drive(ns, func, inp):
    name = PY[func]
    CD = ns["CommandDef"]
    # inject the registry for every case (the defs that read it use it)
    ns["COMMAND_REGISTRY"] = [CD(**d) for d in (inp.get("registry") or [])]
    f = ns[name]
    if func == "localized":
        return f(inp.get("key", ""), inp.get("default", ""))
    if func == "category_label":
        return f(inp.get("category"))
    if func == "describe":
        return f(CD(**inp["cmd"]))
    if func == "desktop_surface_registry":
        return f()
    if func in ("build_commands_by_category", "build_commands", "gateway_available_commands"):
        return f()
    if func == "is_gateway_available":
        ov = inp.get("overrides")
        ov = set(ov) if isinstance(ov, list) else None
        return f(CD(**inp["cmd"]), ov)
    if func == "module_getattr":
        return f(inp.get("name"))
    if func == "normalize_telegram_desc":
        return f(inp.get("desc", ""))
    if func == "truncate_desc":
        return f(inp.get("desc", ""), inp.get("limit", 100))
    if func == "sanitized_rank":
        return f(inp.get("raw_names"))
    if func == "iter_gateway_skills":
        return list(f(inp.get("platform", "")))
    if func == "gateway_bundles":
        return f()
    if func == "completion":
        return f(inp.get("text", ""), inp.get("word", ""), inp.get("display", ""), inp.get("meta"))
    if func == "prefix_completions":
        return list(f(inp.get("rows"), inp.get("partial", ""),
                      skip_exact=inp.get("skip_exact", True)))
    if func == "split_args":
        return f(inp.get("sub_text", ""))
    if func == "dir_completions":
        text_for = lambda fp: seam("text_for", {"path": fp})
        return list(f(inp.get("expanded", ""), inp.get("word", ""), inp.get("limit", 30),
                      text_for, want_dir=inp.get("want_dir")))
    if func == "call_provider":
        provider = (lambda: seam("provider", {})) if inp.get("provider_present") else None
        return f(provider)
    if func == "exec_autoprune":
        ctx = ns["CommandContext"](**(inp.get("ctx") or {}))
        return f(ctx)
    if func == "skill_commands":
        return f()
    if func == "unknown_command_lines":
        return f(inp.get("typed", ""), inp.get("known"))
    raise ValueError("unknown func: %s" % func)


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


def main():
    ns = extract()
    install_stubs(ns)
    REAL = {nm: ns[nm] for nm in WANTED}
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
        # Restore every target def to real (a prior case may have seamed it).
        for nm in REAL:
            ns[nm] = REAL[nm]
        apply_import_gates(inp)
        try:
            ret = drive(ns, func, inp)
        except BaseException as ex:
            ret = {"__raise__": type(ex).__name__, "msg": str(ex)}
        want = {"ret": norm(ret), "trace": CUR["trace"]}
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False))
    if cases == 0:
        print("ORACLE commands_flow: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE commands_flow: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
