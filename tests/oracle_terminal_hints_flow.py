#!/usr/bin/env python3
"""Oracle for the tools/terminal_hints.py UNCLAIMED-remainder FLOW
(src/tools/terminal_hints_flow.c).

AST-extracts the LIVE unclaimed defs (_regex_hint, _missing_command_hint,
_wrapper_cd_hint) plus the nested `hint` closure, seeds the module constants,
and replaces every live regex dependency (re.compile, rx.search, rx.fullmatch,
the module-constant matchers) with recording stubs driven by the SAME per-name
scripts the C harness feeds its universal seams (in.io). Compares the ordered
seam-call trace plus the return value.

Script shapes (both sides): plain value reused per call; {"__seq__": [...]}
consumed per call (last element repeats after exhaustion, empty -> None);
{"__raise__": kind, "msg": text} raises a real exception of that kind; a scripted
null -> None; ABSENT key -> FAIL-CLOSED RuntimeError "no script for <name>"
(recorded as ret, then raised). BaseException kinds pass through every except
site (the ported defs have NO try/except). The nested closure is exec'd
standalone with its free vars (rx, message) bound as per-case globals; the
production _OUTPUT_HINTS instances bind their live captures.

Input: stdin JSON lines from t_port_terminal_hints_flow.
Summary: ORACLE terminal_hints_flow: N cases, M mismatches. Fail-closed on 0 cases.
"""
import ast
import json
import os
import re as _re
import sys
import types
import __future__
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"

CUR = {"func": None, "in": {}, "trace": []}
SEQPOS = {}


def rec(ev):
    CUR["trace"].append(ev)


def script(name, args):
    io = CUR["in"].get("io") or {}
    if name not in io:
        spec = {"__raise__": "RuntimeError", "msg": "no script for %s" % name}
        rec({"ev": name, "args": args, "ret": spec})
        return spec
    sc = io[name]
    val = sc
    if isinstance(sc, dict) and "__seq__" in sc:
        seq = sc["__seq__"]
        idx = SEQPOS.get(name, 0)
        if not seq:
            val = None
        else:
            if idx >= len(seq):
                idx = len(seq) - 1
            val = seq[idx]
            SEQPOS[name] = idx + 1
    rec({"ev": name, "args": args, "ret": val})
    return val


def make_exc(v):
    kind = v.get("__raise__")
    msg = v.get("msg", "")
    if kind == "SystemExit":
        return SystemExit(msg)
    if kind == "KeyboardInterrupt":
        return KeyboardInterrupt(msg)
    if kind == "GeneratorExit":
        return GeneratorExit(msg)
    if kind == "CancelledError":
        import asyncio
        return asyncio.CancelledError(msg)
    builtins = {
        "TypeError": TypeError, "ValueError": ValueError, "AttributeError": AttributeError,
        "KeyError": KeyError, "OSError": OSError, "FileNotFoundError": FileNotFoundError,
        "PermissionError": PermissionError, "RuntimeError": RuntimeError,
        "TimeoutError": TimeoutError, "ImportError": ImportError,
        "ModuleNotFoundError": ModuleNotFoundError, "OverflowError": OverflowError,
        "IndexError": IndexError, "NameError": NameError, "StopIteration": StopIteration,
        "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
        "UnicodeDecodeError": UnicodeDecodeError, "BlockingIOError": BlockingIOError,
        "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
        "ProcessLookupError": ProcessLookupError, "FileExistsError": FileExistsError,
        "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
        "InterruptedError": InterruptedError, "ChildProcessError": ChildProcessError,
        "ConnectionResetError": ConnectionResetError, "ConnectionAbortedError": ConnectionAbortedError,
        "ConnectionRefusedError": ConnectionRefusedError, "EOFError": EOFError,
        "MemoryError": MemoryError, "BufferError": BufferError, "ReferenceError": ReferenceError,
        "AssertionError": AssertionError, "SyntaxError": SyntaxError, "IndentationError": IndentationError,
        "TabError": TabError, "SystemError": SystemError, "UnboundLocalError": UnboundLocalError,
        "ZeroDivisionError": ZeroDivisionError, "FloatingPointError": FloatingPointError,
        "ArithmeticError": ArithmeticError, "LookupError": LookupError, "Warning": Warning,
        "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
        "FutureWarning": FutureWarning, "RuntimeWarning": RuntimeWarning,
        "SyntaxWarning": SyntaxWarning, "ResourceWarning": ResourceWarning,
        "UnicodeError": UnicodeError, "UnicodeEncodeError": UnicodeEncodeError,
        "UnicodeTranslateError": UnicodeTranslateError, "Exception": Exception,
        "BaseException": BaseException, "ExceptionGroup": ExceptionGroup,
    }
    cls = builtins.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(msg)


def materialize(v):
    if isinstance(v, dict) and "__raise__" in v:
        raise make_exc(v)
    return v


def raise_spec(e):
    return {"__raise__": type(e).__name__, "msg": str(e)}


def norm_nums(x):
    if isinstance(x, float) and x.is_integer():
        return int(x)
    if isinstance(x, dict):
        return {k: norm_nums(v) for k, v in x.items()}
    if isinstance(x, list):
        return [norm_nums(v) for v in x]
    return x


class MatchDouble:
    """A re.Match stand-in built from a scripted {groups:[...]} descriptor."""
    def __init__(self, groups):
        self._g = list(groups)

    def group(self, n=1):
        return self._g[n - 1]

    def groups(self):
        return tuple(self._g)

    def __bool__(self):
        return True


def build_match(val):
    """rx.search/fullmatch result for a CONSUMING flow: raise / None / MatchDouble."""
    if isinstance(val, dict) and "__raise__" in val:
        raise make_exc(val)
    if val is None:
        return None
    if isinstance(val, dict) and "groups" in val:
        return MatchDouble(val["groups"])
    return val


def denorm(x):
    if isinstance(x, MatchDouble):
        return {"groups": list(x._g)}
    if isinstance(x, BaseException):
        return raise_spec(x)
    if isinstance(x, dict):
        return {k: denorm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [denorm(v) for v in x]
    return x


# ── live-def extraction ──

PORTED = ("_regex_hint", "_missing_command_hint", "_wrapper_cd_hint")
CONSTS = ("_SCAN_CHARS", "_MISSING_COMMAND_HINTS", "_OUTPUT_HINTS",
          "_EXIT_CODE_HINTS", "_WRAPPER_CD_FAILURE", "_PASSTHROUGH_CONSUMERS",
          "_MASKING_SHAPES", "_READONLY_HEADS", "_FAILURE_SHAPES")


def extract():
    path = os.path.join(QUARRY, "tools", "terminal_hints.py")
    src = open(path, encoding="utf-8").read()
    modname = "th_flow_extracted"
    ns = {"__name__": modname, "__file__": path,
          "re": _re, "Callable": __import__("typing").Callable,
          "Optional": __import__("typing").Optional}
    sys.modules.setdefault(modname, types.ModuleType(modname))
    flags = __future__.annotations.compiler_flag
    tree = ast.parse(src)

    def ex(node):
        exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec", flags=flags), ns)

    # _SCAN_CHARS first (simple int)
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
                isinstance(t, ast.Name) and t.id == "_SCAN_CHARS" for t in node.targets):
            ex(node)
    # the two leaf defs the constants reference
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in ("_regex_hint", "_missing_command_hint"):
            ex(node)
    # constants (need re + _regex_hint + _missing_command_hint)
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if any(isinstance(t, ast.Name) and t.id in CONSTS for t in tgts):
                ex(node)
    # remaining ported def
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_wrapper_cd_hint":
            ex(node)
    # nested `hint` closure inside _regex_hint -> exec standalone (free vars become globals)
    hint_node = None
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_regex_hint":
            for sub in ast.walk(node):
                if isinstance(sub, ast.FunctionDef) and sub.name == "hint":
                    hint_node = sub
    if hint_node is None:
        raise RuntimeError("nested hint closure not found")
    ex(hint_node)

    missing = (set(PORTED) | set(CONSTS) | {"hint"}) - set(ns)
    if missing:
        raise RuntimeError("extraction missing: %s" % sorted(missing))
    return ns


NS = extract()
# production captures from the live _OUTPUT_HINTS closures
PROD_NAMES = ["gh_unknown_json_field", "merge_conflict", "command_not_found",
              "module_not_found", "already_exists", "gh_rate_limit", "permission_denied"]
PROD = {}
for nm, fn in zip(PROD_NAMES, NS["_OUTPUT_HINTS"]):
    cells = {n: c.cell_contents for n, c in zip(fn.__code__.co_freevars, fn.__closure__)}
    PROD[nm] = {"pattern": cells["rx"].pattern, "flags": cells["rx"].flags,
                "message": cells["message"]}
REAL_CONSTS = {k: NS[k] for k in CONSTS}


def mat_message(m):
    """Materialize a message node: str -> str; callable marker -> real/recorder."""
    if isinstance(m, dict) and "__callable__" in m:
        if m["__callable__"] == "missing_command_hint":
            return NS["_missing_command_hint"]
        def custom(g1):
            return materialize(script("message_call", {"group1": g1}))
        return custom
    return m


def proj_message(m):
    return m  # str or {"__callable__": name} echoed verbatim


def install_case(func, inp):
    # restore real constants each case
    for k, v in REAL_CONSTS.items():
        NS[k] = v
    # re.compile seam (regex_hint factory)
    NS["re"] = types.SimpleNamespace(
        compile=lambda pattern, flags=0: materialize(script("re.compile", {"pattern": pattern, "flags": flags})),
        MULTILINE=_re.MULTILINE, UNICODE=_re.UNICODE)
    # rx double for the hint closure (search -> MatchDouble)
    def rx_search(output):
        return build_match(script("rx.search", {"output": output}))
    NS["rx"] = types.SimpleNamespace(search=rx_search)
    # message binding
    if func == "hint":
        NS["message"] = mat_message(inp.get("message"))
    elif func.startswith("hint_"):
        nm = func[len("hint_"):]
        NS["message"] = PROD[nm]["message"]
    # _WRAPPER_CD_FAILURE double: match mode for wrapper_cd_hint, raw for fullmatch drive
    if func == "wrapper_cd_hint":
        NS["_WRAPPER_CD_FAILURE"] = types.SimpleNamespace(
            fullmatch=lambda last_line: build_match(script("rx.fullmatch", {"last_line": last_line})))
    elif func == "wrapper_cd_fullmatch":
        NS["_WRAPPER_CD_FAILURE"] = types.SimpleNamespace(
            fullmatch=lambda last_line: materialize(script("rx.fullmatch", {"last_line": last_line})))
    # direct matcher drives (raw mode)
    if func == "failure_shapes_search":
        NS["_FAILURE_SHAPES"] = types.SimpleNamespace(
            search=lambda window: materialize(script("failure_shapes.search", {"window": window})))
    if func in ("masking_pipe_search", "masking_fallback_search"):
        pipe = types.SimpleNamespace(
            search=lambda cmd: materialize(script("masking_pipe.search", {"cmd": cmd})))
        fall = types.SimpleNamespace(
            search=lambda cmd: materialize(script("masking_fallback.search", {"cmd": cmd})))
        NS["_MASKING_SHAPES"] = [(pipe, "pipe-note"), (fall, "fall-note")]


def run_flow(fn):
    try:
        ret = fn()
        return {"ret": denorm(ret)}
    except BaseException as e:
        return {"ret": raise_spec(e)}


def drive(func, inp):
    install_case(func, inp)
    if func == "regex_hint":
        pattern = inp.get("pattern", "")
        message = mat_message(inp.get("message"))
        flags = inp.get("flags", 0)
        def call():
            NS["_regex_hint"](pattern, message, flags)
            # project the closure to its captures (== the inputs; factory is identity)
            return {"pattern": pattern, "message": proj_message(inp.get("message")), "flags": flags}
        return run_flow(call)
    if func == "hint" or func.startswith("hint_"):
        command = inp.get("command", "")
        output = inp.get("output")
        return run_flow(lambda: NS["hint"](command, output))
    if func == "missing_command_hint":
        return run_flow(lambda: NS["_missing_command_hint"](inp.get("missing", "")))
    if func == "wrapper_cd_hint":
        return run_flow(lambda: NS["_wrapper_cd_hint"](inp.get("output")))
    if func == "failure_shapes_search":
        return run_flow(lambda: NS["_FAILURE_SHAPES"].search(inp.get("window", "")))
    if func == "masking_pipe_search":
        return run_flow(lambda: NS["_MASKING_SHAPES"][0][0].search(inp.get("cmd", "")))
    if func == "masking_fallback_search":
        return run_flow(lambda: NS["_MASKING_SHAPES"][1][0].search(inp.get("cmd", "")))
    if func == "wrapper_cd_fullmatch":
        return run_flow(lambda: NS["_WRAPPER_CD_FAILURE"].fullmatch(inp.get("last_line", "")))
    raise ValueError("unknown func: %s" % func)


def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


def main():
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
        inp = norm_nums(record.get("in") or {})
        got = record.get("out") or {}
        cases += 1
        CUR["func"] = func
        CUR["in"] = inp
        CUR["trace"] = []
        SEQPOS.clear()
        try:
            want = drive(func, inp)
        except BaseException as e:
            want = {"ret": raise_spec(e)}
        want["trace"] = CUR["trace"]
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE terminal_hints_flow: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())