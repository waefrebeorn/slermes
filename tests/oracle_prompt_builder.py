#!/usr/bin/env python3
"""Oracle for port_prompt_builder.c -- drives the LIVE agent/prompt_builder.py
defs. The config / file-scan half is not imported; the pure defs are
ast-extracted together with the module constants they read.
"""
import ast
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\agent\prompt_builder.py"
WANTED = ["_strip_yaml_frontmatter", "format_steer_marker",
          "_format_backend_probe", "_plugin_backend_is_remote"]


def extract():
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    ns = {"frozenset": frozenset, "set": set, "Optional": object, "Any": object,
          "bool": bool, "str": str}
    # constants first (STEER_MARKER_OPEN/CLOSE, _REMOTE_TERMINAL_BACKENDS)
    for node in tree.body:
        if isinstance(node, ast.Assign):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    # _plugin_backend_attr is a registry lookup; the harness passes the value
    ns["_STUB_REMOTE"] = False
    ns["_plugin_backend_attr"] = lambda backend, key, default=None: ns["_STUB_REMOTE"]
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    strips = {
        "no-frontmatter": "# Title\nbody\n",
        "simple": "---\ntitle: x\n---\nbody\n",
        "closing-only": "---\nbody\n",
        "bom": "\ufeff---\ntitle: x\n---\nbody\n",
        "bom-only": "\ufeff# Title\n",
        "empty-after": "---\ntitle: x\n---\n",
        "newlines-after": "---\na: 1\n---\n\n\nbody\n",
        "dash-in-body": "intro\n---\nbody\n",
        "not-closed": "---\nno closing fence\n",
        "empty": "", "null": None,
        "four-dashes": "----\nbody\n",
    }
    markers = {"simple": "please stop", "multi": "line one\nline two", "empty": ""}
    probes = {
        "both": "os=Linux\nkernel=5.15\n", "os-only": "os=Windows\n",
        "kernel-only": "kernel=6.1\n", "unknown": "os=unknown\nkernel=unknown\n",
        "mixed": "os=unknown\nkernel=6.1\n",
        "spaces": "  os = Linux  \n  kernel = 5.15  \n",
        "later-wins": "os=First\nos=Second\n", "no-eq": "no equals here\n",
        "empty": "", "crlf": "os=Linux\r\nkernel=5.15\r\n",
        "other-keys": "name=x\nversion=2\n",
    }
    remotes = {
        "local": ("local", 0), "remote-set": ("docker", 0), "ssh": ("ssh", 0),
        "remote-attr": ("custom", 1), "unknown-local": ("custom", 0),
        "empty": ("", 1), "modal": ("modal", 0),
    }

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
            if fn == "_strip_yaml_frontmatter":
                # the live fn raises AttributeError on None (None.lstrip);
                # the C port returns NULL, which is the faithful equivalent
                try:
                    want = ns["_strip_yaml_frontmatter"](strips[label])
                except AttributeError:
                    want = None
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _strip_yaml_frontmatter case={label} C={got!r} PY={want!r}")

            elif fn == "format_steer_marker":
                want = ns["format_steer_marker"](markers[label])
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH format_steer_marker case={label} C={got!r} PY={want!r}")

            elif fn == "_format_backend_probe":
                want = ns["_format_backend_probe"](probes[label])
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _format_backend_probe case={label} C={got!r} PY={want!r}")

            elif fn == "_plugin_backend_is_remote":
                backend, attr = remotes[label]
                # attr_is_remote stands in for the plugin registry lookup; the
                # stub follows the harness's value so the two agree
                ns["_STUB_REMOTE"] = bool(attr)
                want = bool(ns["_plugin_backend_is_remote"](backend))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _plugin_backend_is_remote case={label} C={got!r} PY={want!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE prompt_builder: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
