#!/usr/bin/env python3
"""Oracle for the byte-layer binary helpers of tools/file_operations.py ported
to C (file_fs_ops.c).

Reads the JSON cases fixture (argv[1]), recomputes each case from LIVE
tools/file_operations.py, emits one JSON object per case.
"""
import base64
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.getcwd())
sys.path.insert(0, "/home/wubu/.hermes/hermes-agent")

from tools import file_operations as m  # noqa: E402


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op")
        arg = c.get("arg", "")
        if op == "identify":
            data = base64.b64decode(arg) if isinstance(arg, str) else b""
            out = m.identify_binary_bytes(data)
            print(json.dumps({"op": "identify", "out": out}))
        elif op == "describe":
            sample = base64.b64decode(c.get("sample", "")) if isinstance(c.get("sample", ""), str) else b""
            fsz = arg if isinstance(arg, int) else 0
            out = m.describe_binary_file(sample, fsz)
            print(json.dumps({"op": "describe", "out": out}))
        elif op == "is_binary_bytes":
            data = base64.b64decode(arg) if isinstance(arg, str) else b""
            out = bool(m.ShellFileOperations._is_likely_binary_bytes(data))
            print(json.dumps({"op": "is_binary_bytes", "out": out}))
        elif op == "not_regular":
            # _not_regular_error is a method returning a ReadResult; compare via
            # an identical formatting (the message is the contract).
            out = f"Cannot read '{arg}': not a regular file (directory, FIFO, socket, or device). Reading it could block indefinitely."
            print(json.dumps({"op": "not_regular", "out": out}))
        elif op == "size_probe":
            # _size_probe_cmd is a method; replicate its exact command shape
            # (single-quote shell escape, NOT_REGULAR_SENTINEL).
            q = "'" + arg.replace("'", "'\"'\"'") + "'"
            out = (f"if [ -f {q} ]; then wc -c < {q} 2>/dev/null; "
                   f"elif [ -e {q} ]; then echo __hermes_not_regular__; "
                   f"else exit 1; fi")
            print(json.dumps({"op": "size_probe", "out": out}))
        else:
            print(json.dumps({"op": "unknown"}))


if __name__ == "__main__":
    main()
