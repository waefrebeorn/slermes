#!/usr/bin/env python3
"""Oracle for tools/bot_desktop/rfb_filter.py::_message_length.

The module imports only stdlib, but the live class's __init__ / reader are
runtime state; the pure framer reads only self._buf and module constants. So we
ast-extract the LIVE `_message_length` method plus the module constants it uses
(_FIXED, _SET_ENCODINGS, _CLIENT_CUT_TEXT, _FENCE, _SET_DESKTOP_SIZE,
_MAX_CUT_TEXT) and drive a minimal fake self with _buf = the hex-decoded bytes.

Mapping to C sentinels: int length -> itself; None -> -1; ValueError
(unknown type) -> -2; ValueError (clipboard too large) -> -3.
"""
import ast
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/tools/bot_desktop/rfb_filter.py"


def load(path):
    src = open(path, encoding="utf-8").read()
    ns = {}
    for n in ast.parse(src).body:
        if isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name):
            if n.targets[0].id in ("_FIXED", "_SET_ENCODINGS", "_CLIENT_CUT_TEXT",
                                   "_FENCE", "_SET_DESKTOP_SIZE", "_MAX_CUT_TEXT"):
                mod = ast.Module(body=[n], type_ignores=[])
                exec(compile(ast.fix_missing_locations(mod), path, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name == "_message_length":
            mod = ast.Module(body=[n], type_ignores=[])
            exec(compile(ast.fix_missing_locations(mod), path, "exec"), ns)
        elif isinstance(n, ast.ClassDef):
            for m in n.body:
                if isinstance(m, ast.FunctionDef) and m.name == "_message_length":
                    mod = ast.Module(body=[m], type_ignores=[])
                    exec(compile(ast.fix_missing_locations(mod), path, "exec"), ns)
    return ns


ns = load(SRC)
FN = ns["_message_length"]


class FakeSelf:
    pass


def hex_to_bytes(hx):
    hx = hx.strip()
    out = bytearray()
    for j in range(0, len(hx) - 1, 2):
        out.append(int(hx[j:j + 2], 16))
    return bytes(out)


def main() -> int:
    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE rfb_filter: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        if c.get("tag") != "rfb":
            continue
        self_ = FakeSelf()
        self_._buf = bytearray(hex_to_bytes(c["hex"]))
        try:
            want = FN(self_)
            if want is None:
                want = -1
        except ValueError as e:
            want = -3 if "clipboard message too large" in str(e) else -2
        if want != c["out"]:
            mism += 1
            print(f"MISMATCH {c.get('case')} hex={c['hex']}: C={c['out']} PY={want}")
    print(f"ORACLE rfb_filter: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
