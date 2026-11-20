#!/usr/bin/env python3
"""Oracle for commands_flow3.c — the hermes_cli/commands.py remainder FLOW
surface beyond the flow1 registry cluster (10 live defs).

Drives the LIVE functions with SCRIPTED I/O: defs are AST-extracted from the
quarry (no heavy hermes_cli import), CommandDef is exec'd as a real dataclass,
and every foreign effect (cmd.describe, agent.i18n.t, the claimed siblings
_resolve_config_gates / _is_gateway_available / _iter_plugin_command_entries,
the hermes_cli.config / hermes_cli.plugins seams) is replaced by a recording
stub returning the SAME scripted results the C harness feeds its universal
seam. The command registry is injected per case. Compares the ordered DECISION
SEQUENCE + return against the harness output.

Fail-closed: a missing/empty seam script returns a RuntimeError raise spec;
`except Exception` sites swallow the full Exception hierarchy but pass the
BaseException trio. Import gates (fixture "import_gates") set sys.modules[mod]
=None so the live from-import raises the identical ModuleNotFoundError the C
port parks.

Input: stdin JSON lines from t_port_commands_flow3:
    {"func":..., "in":{...}, "out":{"ret":..., "trace":[...]}}
Summary: ORACLE commands_flow3: N cases, M mismatches. Exit 2 on seed failure.
"""
import ast
import json
import os
import sys
import types
from dataclasses import asdict, dataclass, field, is_dataclass

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("HOME", os.path.dirname(_REPO))
os.environ.setdefault("USERPROFILE", os.path.dirname(_REPO))

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC_COMMANDS = os.path.join(QUARRY, "hermes_cli", "commands.py")

# harness func name -> live python name
PY = {
    "infer_argument_mode": "infer_argument_mode",
    "command_desktop_meta": "command_desktop_meta",
    "resolve_command": "resolve_command",
    "build_description": "_build_description",
    "is_gateway_known_command": "is_gateway_known_command",
    "is_interrupt_then_dispatch": "is_interrupt_then_dispatch",
    "should_bypass_active_session": "should_bypass_active_session",
    "resolve_config_gates": "_resolve_config_gates",
    "gateway_help_lines": "gateway_help_lines",
    "iter_plugin_command_entries": "_iter_plugin_command_entries",
}
WANTED = set(PY.values())
DEF_HOME = {nm: (SRC_COMMANDS, None) for nm in WANTED}
CONSTS = {SRC_COMMANDS: ["_PROSE_HINTS"]}
CLASSES = {SRC_COMMANDS: ["CommandDef"]}

CUR = {"trace": [], "scripts": {}, "idx": {}}

BASE_EXC = {"SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
            "GeneratorExit": GeneratorExit}
BUILTIN_EXC = {
    "Exception": Exception, "BaseException": BaseException,
    "RuntimeError": RuntimeError, "ValueError": ValueError, "TypeError": TypeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "NameError": NameError,
    "LookupError": LookupError, "IndexError": IndexError,
    "StopIteration": StopIteration, "StopAsyncIteration": StopAsyncIteration,
    "OSError": OSError, "FileNotFoundError": FileNotFoundError,
    "PermissionError": PermissionError, "NotADirectoryError": NotADirectoryError,
    "IsADirectoryError": IsADirectoryError, "FileExistsError": FileExistsError,
    "TimeoutError": TimeoutError, "ImportError": ImportError,
    "ModuleNotFoundError": ModuleNotFoundError, "AssertionError": AssertionError,
    "ArithmeticError": ArithmeticError, "ZeroDivisionError": ZeroDivisionError,
    "OverflowError": OverflowError, "FloatingPointError": FloatingPointError,
    "UnicodeError": UnicodeError, "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeEncodeError": UnicodeEncodeError, "UnicodeTranslateError": UnicodeTranslateError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "MemoryError": MemoryError, "BufferError": BufferError, "EOFError": EOFError,
    "ReferenceError": ReferenceError, "SystemError": SystemError,
    "SyntaxError": SyntaxError, "IndentationError": IndentationError,
    "TabError": TabError, "UnboundLocalError": UnboundLocalError,
    "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "BlockingIOError": BlockingIOError,
    "ChildProcessError": ChildProcessError, "ProcessLookupError": ProcessLookupError,
    "InterruptedError": InterruptedError,
    "Warning": Warning, "UserWarning": UserWarning,
    "DeprecationWarning": DeprecationWarning, "PendingDeprecationWarning": PendingDeprecationWarning,
    "SyntaxWarning": SyntaxWarning, "RuntimeWarning": RuntimeWarning,
    "FutureWarning": FutureWarning, "ImportWarning": ImportWarning,
    "UnicodeWarning": UnicodeWarning, "BytesWarning": BytesWarning,
    "ResourceWarning": ResourceWarning, "EncodingWarning": EncodingWarning,
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
    if o is None or isinstance(o, (bool, int, str, float)):
        return o
    if isinstance(o, BaseException):
        return str(o)
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
STUB_MODS = {}


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

    ns["dataclass"] = dataclass
    ns["field"] = field
    ns["asdict"] = asdict

    tree = ast.parse(open(SRC_COMMANDS, encoding="utf-8").read())

    # CommandDef first (registry materialization needs the real dataclass).
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in CLASSES[SRC_COMMANDS]:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC_COMMANDS, "exec"), ns)

    # selective module constants (plain-Name Assign / AnnAssign).
    for node in tree.body:
        tgt = None
        if isinstance(node, ast.Assign) and all(isinstance(t, ast.Name) for t in node.targets):
            tgt = [t.id for t in node.targets]
        elif isinstance(node, ast.AnnAssign) and node.value and isinstance(node.target, ast.Name):
            tgt = [node.target.id]
        if tgt and any(t in CONSTS[SRC_COMMANDS] for t in tgt):
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC_COMMANDS, "exec"), ns)
    missing_c = [n for n in CONSTS[SRC_COMMANDS] if n not in ns]
    if missing_c:
        print("FATAL missing constants: %s" % missing_c)
        sys.exit(2)

    # wanted defs (strip decorators AND annotations so no typing imports are needed).
    for name in WANTED:
        node = next((n for n in tree.body
                     if isinstance(n, ast.FunctionDef) and n.name == name), None)
        if node is None:
            print("FATAL def not found: %s" % name)
            sys.exit(2)
        node.decorator_list = []
        node.returns = None
        for a in list(node.args.args) + list(node.args.kwonlyargs):
            a.annotation = None
        if node.args.vararg:
            node.args.vararg.annotation = None
        if node.args.kwarg:
            node.args.kwarg.annotation = None
        node = ast.fix_missing_locations(ast.Module(body=[node], type_ignores=[]))
        exec(compile(node, SRC_COMMANDS, "exec"), ns)

    missing = WANTED - set(ns)
    if missing:
        print("FATAL missing defs: %s" % sorted(missing))
        sys.exit(2)
    NS = ns
    return ns


def stub_module(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    STUB_MODS[name] = m
    sys.modules[name] = m
    return m


def install_stubs(ns):
    CD = ns["CommandDef"]
    CD.describe = lambda self: seam("describe", {"cmd": asdict(self)})
    ns["t"] = lambda key, **kw: seam("t", {"key": key, **kw})
    ns["is_truthy_value"] = lambda value, default=False: seam(
        "is_truthy_value", {"value": value, "default": default})
    stub_module("hermes_cli.config",
                cfg_get=lambda cfg, *parts: seam("cfg_get", {"cfg": cfg, "parts": list(parts)}),
                read_raw_config=lambda: seam("read_raw_config", {}))
    stub_module("hermes_cli.plugins",
                get_plugin_commands=lambda: seam("get_plugin_commands", {}))


# Per-composition seam stubs (installed in drive() for the composing func only).
def seam_gates():
    return (lambda: seam("_resolve_config_gates", {}))


def seam_avail():
    return (lambda cmd, overrides=None: seam(
        "_is_gateway_available", {"cmd": asdict(cmd), "overrides": overrides}))


def seam_plugin_entries():
    return (lambda: seam("_iter_plugin_command_entries", {}))


def apply_import_gates(inp):
    gates = inp.get("import_gates") or {}
    for name in ("hermes_cli.config", "hermes_cli.plugins"):
        sys.modules[name] = None if gates.get(name) else STUB_MODS[name]


def seed_globals(ns, inp):
    reg = [ns["CommandDef"](**d) for d in (inp.get("registry") or [])]
    ns["COMMAND_REGISTRY"] = reg
    ns["_COMMAND_LOOKUP"] = {
        k: c for c in reg for k in (c.name, *c.aliases)}
    ns["GATEWAY_KNOWN_COMMANDS"] = frozenset(
        n for c in reg if (not c.cli_only) or c.gateway_config_gate
        for n in (c.name, *c.aliases))
    return reg


def drive(ns, func, inp):
    name = PY[func]
    seed_globals(ns, inp)
    f = ns[name]
    if func == "infer_argument_mode":
        return f(ns["CommandDef"](**inp["cmd"]))
    if func == "command_desktop_meta":
        return f(ns["CommandDef"](**inp["cmd"]))
    if func == "resolve_command":
        return f(inp.get("name"))
    if func == "build_description":
        return f(ns["CommandDef"](**inp["cmd"]))
    if func == "is_gateway_known_command":
        ns["_iter_plugin_command_entries"] = seam_plugin_entries()
        return f(inp.get("name"))
    if func == "is_interrupt_then_dispatch":
        return f(inp.get("name"))
    if func == "should_bypass_active_session":
        return f(inp.get("name"))
    if func == "resolve_config_gates":
        return f()
    if func == "gateway_help_lines":
        ns["_resolve_config_gates"] = seam_gates()
        ns["_is_gateway_available"] = seam_avail()
        allowed = inp.get("allowed")
        return f(None if allowed is None else set(allowed))
    if func == "iter_plugin_command_entries":
        return f()
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
        print("ORACLE commands_flow3: 0 cases (fail-closed)")
        sys.exit(2)
    print("ORACLE commands_flow3: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
