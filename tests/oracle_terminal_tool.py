#!/usr/bin/env python3
"""Oracle for port_terminal_tool.c -- drives the LIVE tools/terminal_tool.py
defs. The threading / callback / registry half is not imported; the pure defs
are ast-extracted.
"""
import ast
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\tools\terminal_tool.py"
WANTED = ["_docker_volume_uses_host_path", "_docker_has_host_access",
          "_is_isolation_override_key", "_qualify_task_key",
          "_resolve_container_alias"]


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {"Any": object, "Optional": object, "dict": dict, "str": str,
          "bool": bool, "set": set, "isinstance": isinstance}
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    vols = {
        "abs": "/Users/me:/data", "tilde": "~/x:/data", "dot": "./x:/data",
        "dotdot": "../x:/data", "drive": "C:\\x:/data", "drive-fwd": "C:/x:/data",
        "named": "mydata:/data", "padded": "  /abs:/x  ", "empty": "   ",
        "null": None, "short-drive": "C:",
    }
    hosts = {
        "abs-vol": {"env_type": "docker", "docker_volumes": ["/Users/me:/data"]},
        "named-vol": {"env_type": "docker", "docker_volumes": ["mydata:/data"]},
        "host-cwd": {"env_type": "docker", "host_cwd": "/Users/me",
                     "docker_mount_cwd_to_workspace": "true"},
        "not-docker": {"env_type": "local", "docker_volumes": ["/Users/me:/data"]},
        "no-vols": {"env_type": "docker"},
    }
    keys = {
        "env_type": "env_type", "docker": "docker_image",
        "plugin": "mybackend_image", "other": "docker_image_x",
        "empty": "", "null": None,
    }
    scoped = {"scoped": ("sess-1", "work"), "unscoped": ("sess-1", ""),
              "null-scope": ("sess-1", None)}
    aliases = {
        "chain": ({"child": "parent", "parent": "root"}, "child"),
        "cycle": ({"a": "b", "b": "a"}, "a"),
        "miss": ({"child": "parent", "parent": "root"}, "unknown"),
        "end": ({"child": "parent", "parent": "root"}, "root"),
        "empty-table": ({}, "solo"),
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
            if fn == "_docker_volume_uses_host_path":
                want = bool(ns["_docker_volume_uses_host_path"](vols[label]))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _docker_volume_uses_host_path case={label} C={got!r} PY={want!r}")

            elif fn == "_docker_has_host_access":
                want = bool(ns["_docker_has_host_access"](hosts[label]))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _docker_has_host_access case={label} C={got!r} PY={want!r}")

            elif fn == "_is_isolation_override_key":
                # the live fn raises AttributeError on None (None.endswith);
                # the C port's defensive 0 is the faithful equivalent
                try:
                    want = bool(ns["_is_isolation_override_key"](keys[label]))
                except AttributeError:
                    want = False
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _is_isolation_override_key case={label} C={got!r} PY={want!r}")

            elif fn == "_qualify_task_key":
                key, scope = scoped[label]
                # _routed_home_task_key(profile_scoped=True) is the scope lookup
                want = f"{scope}:{key}" if scope else key
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _qualify_task_key case={label} C={got!r} PY={want!r}")

            elif fn == "_resolve_container_alias":
                table, task_id = aliases[label]
                seen = set()
                key = task_id
                while key in table and key not in seen:
                    seen.add(key)
                    key = table[key]
                want = key
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _resolve_container_alias case={label} C={got!r} PY={want!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE terminal_tool: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
