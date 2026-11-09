#!/usr/bin/env python3
"""Oracle for port_methods_tools.c -- drives the LIVE
tui_gateway/methods_tools.py, tui_gateway/user_messages.py and
tui_gateway/server.py defs. The RPC dispatch / session registry half is not
imported; the pure defs are ast-extracted.
"""
import ast
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

TOOLS = r"C:\Users\eman5\hermes-agent\tui_gateway\methods_tools.py"
MSGS = r"C:\Users\eman5\hermes-agent\tui_gateway\user_messages.py"
SERVER = r"C:\Users\eman5\hermes-agent\tui_gateway\server.py"

WANTED = {
    TOOLS: ["_str_arg", "_mcp_plugin_write_error", "_busy_error"],
    MSGS: ["busy_message"],
    SERVER: ["_err"],
}


def extract():
    ns = {"dict": dict, "str": str, "int": int, "bool": bool, "Any": object,
          "Optional": object, "frozenset": frozenset}
    for path, names in WANTED.items():
        src = open(path, encoding="utf-8").read()
        for node in ast.parse(src).body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                try:
                    exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)
                except Exception:
                    pass
        for node in ast.parse(src).body:
            if isinstance(node, ast.FunctionDef) and node.name in names:
                exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), ns)
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    strs = {
        "plain": ({"name": "  hello  "}, "name"),
        "empty": ({"name": ""}, "name"),
        "null": ({"name": None}, "name"),
        "missing": ({"other": "x"}, "name"),
        "no-map": ({}, "name"),
    }
    busy = {"replying": ("/model", False), "compressing": ("/compress", True),
            "no-slash": ("model", False), "many-slashes": ("///loop", False)}
    errs = {
        "no-data": ("7", 4001, "no active session", None),
        "with-data": ("7", 4064, "profile 'x' not found", "extra"),
        "escapes": (None, 4001, 'quote" and \\ slash', None),
    }
    plugins = {
        "provided": ("github", {"github": "github-mcp"}),
        "not-provided": ("github", {"other": "thing"}),
        "no-plugins": ("github", {}),
    }
    sessions = {
        "running": ({"running": True}, "/model"),
        "compressing": ({"running": True, "_manual_compress_active": True}, "/compress"),
        "idle": ({"running": False}, "/model"),
        "no-session": ({}, "/model"),
    }

    def as_json(o):
        """compact_json of the live dict, so the C envelope can be compared."""
        import json as _j
        return _j.dumps(o, ensure_ascii=False, separators=(",", ":"))

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        fn = c.get("fn")
        label = c.get("case")
        cases += 1

        try:
            if fn == "_str_arg":
                p, k = strs[label]
                want = ns["_str_arg"](p, k)
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _str_arg case={label} C={got!r} PY={want!r}")

            elif fn == "busy_message":
                cmd, comp = busy[label]
                want = ns["busy_message"](cmd, comp)
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH busy_message case={label} C={got!r} PY={want!r}")

            elif fn == "_err":
                rid, code, msg, data = errs[label]
                want = as_json(ns["_err"](rid, code, msg, data))
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _err case={label}\n  C={got!r}\n  PY={want!r}")

            elif fn == "_mcp_plugin_write_error":
                name, pl = plugins[label]
                r = ns["_mcp_plugin_write_error"]("3", name, pl)
                want = as_json(r) if r is not None else ""
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _mcp_plugin_write_error case={label} C={got!r} PY={want!r}")

            elif fn == "_busy_error":
                se, cmd = sessions[label]
                r = ns["_busy_error"]("5", se, cmd)
                want = as_json(r) if r is not None else ""
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _busy_error case={label} C={got!r} PY={want!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE methods_tools: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
