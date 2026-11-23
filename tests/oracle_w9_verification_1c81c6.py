#!/usr/bin/env python3
"""Differential oracle for gate w9_verification_1c81c6 (agent/verification_stop.py).

AST-extracts the three policy helpers from the quarry and drives them with a stubbed
os.environ (the env value is injected explicitly, never set on the process) and a
stubbed messaging-surface probe. Compares 'out' against the C port.
"""
import __future__
import ast
import builtins
import json
import os as _real_os
import sys
import types
from pathlib import Path

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "verification_stop.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
sys.path.insert(0, str(QUARRY))
SRC = str(QUARRY / "agent/verification_stop.py")
NAME = "w9_verification_1c81c6"
PORTED = ["_is_non_code_path", "_format_changed_paths", "verify_on_stop_enabled"]
CONSTS = ["_NON_CODE_VERIFY_EXTENSIONS", "_NON_CODE_VERIFY_FILENAMES",
          "_FALSY_TOKENS", "_TRUTHY_TOKENS", "_MAX_CHANGED_PATHS_IN_NUDGE"]

_TREE = ast.parse(Path(SRC).read_text(encoding="utf-8"))
_FLAGS = __future__.annotations.compiler_flag


def _compile(node):
    return compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                   flags=_FLAGS, dont_inherit=True)


_CODE, _CCODE = {}, {}
for _n in _TREE.body:
    if isinstance(_n, ast.FunctionDef) and _n.name in PORTED:
        _CODE[_n.name] = _compile(_n)
    elif isinstance(_n, (ast.Assign, ast.AnnAssign)):
        _t = _n.targets[0] if isinstance(_n, ast.Assign) else _n.target
        if isinstance(_t, ast.Name) and _t.id in CONSTS:
            _CCODE[_t.id] = _compile(_n)
_bad = (set(PORTED) - set(_CODE)) | (set(CONSTS) - set(_CCODE))
if _bad:
    print(f"ORACLE {NAME}: FATAL missing: {sorted(_bad)}", file=sys.stderr)
    sys.exit(1)

_MOD = types.ModuleType("w9vs_live_verification_stop")
_MOD.__dict__.update({"__name__": "w9vs_live_verification_stop", "__builtins__": builtins,
                      "Path": Path, "Any": object, "Iterable": object})
sys.modules["w9vs_live_verification_stop"] = _MOD
_NS = _MOD.__dict__
for _nm, _c in _CCODE.items():
    exec(_c, _NS)
for _nm, _c in _CODE.items():
    exec(_c, _NS)

_MESSAGING = False
_NS["os"] = types.SimpleNamespace(environ={})
_NS["_session_is_messaging_surface"] = lambda: _MESSAGING


def drive(rec):
    global _MESSAGING
    inp, func = rec["in"], rec["func"]
    if func == "non_code":
        return _NS["_is_non_code_path"](inp.get("path", ""))
    if func == "format_paths":
        return _NS["_format_changed_paths"](inp.get("paths") or [])
    if func == "enabled":
        env = inp.get("env")
        environ = {}
        if env is not None:
            environ["HERMES_VERIFY_ON_STOP"] = env
        _NS["os"].environ = environ
        _MESSAGING = bool(inp.get("messaging_surface", False))
        return _NS["verify_on_stop_enabled"](inp.get("config"))
    raise KeyError(func)


def main():
    n = mism = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        try:
            out = drive(rec)
        except BaseException as e:
            out = {"__raise__": type(e).__name__}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:800]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:800]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
