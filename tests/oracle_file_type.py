#!/usr/bin/env python3
"""Oracle for extension-based file-type detection (port src/tools/file_fs_ops.c
file_fs_ops_is_image / file_fs_ops_is_likely_binary). The harness passes
EXTENSION TOKENS (no real files), so both sides reduce to extension-set
membership. Direct import hits the gate interpreter's missing ruamel, so
ast-extract the live methods + extension sets:
  image:  _is_image(path) = splitext/1.lower() in IMAGE_EXTENSIONS
  binary: _is_likely_binary(path) = has_binary_extension = ext in BINARY_EXTENSIONS
"""
import ast
import json
import os
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def extract(path, names, seeds):
    src = open(path, encoding="utf-8").read()
    g = dict(seeds)
    for n in ast.parse(src).body:
        body = n.body if isinstance(n, ast.ClassDef) else [n]
        for m in body:
            if isinstance(m, ast.FunctionDef) and m.name in names:
                exec(compile(ast.Module(body=[m], type_ignores=[]), "m", "exec"), g)
            elif isinstance(m, ast.Assign):
                for tar in m.targets:
                    if isinstance(tar, ast.Name) and tar.id in names:
                        exec(compile(ast.Module(body=[m], type_ignores=[]), "m", "exec"), g)
    return g


def main() -> int:
    fo = extract("C:/Users/eman5/hermes-agent/tools/file_operations.py",
                 {"_is_image", "IMAGE_EXTENSIONS"}, {"os": os})
    be = extract("C:/Users/eman5/hermes-agent/tools/binary_extensions.py",
                 {"has_binary_extension", "BINARY_EXTENSIONS",
                  "_has_extension_in", "_lower_suffix", "_strip_sidecar_marker",
                  "_SQLITE_SIDECAR_MARKERS"}, {"os": os})
    is_image = fo["_is_image"]
    has_be = None
    if "has_binary_extension" in be:
        has_be = be["has_binary_extension"]
    else:
        def has_be(p):
            return os.path.splitext(str(p or ""))[1].lower() in be["BINARY_EXTENSIONS"]
    total, mism = 0, 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        op = c.get("op")
        val = c.get("in") or ""
        if op == "image":
            want = bool(is_image(None, val))
            if want != bool(c.get("out")):
                mism += 1
                print(f"MISMATCH image {val!r} C={c.get('out')} PY={want}")
        elif op == "binary":
            want = bool(has_be(val))
            if want != bool(c.get("out")):
                mism += 1
                print(f"MISMATCH binary {val!r} C={c.get('out')} PY={want}")
    if total == 0:
        print("ORACLE file_type: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE file_type: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
