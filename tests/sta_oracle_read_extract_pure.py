#!/usr/bin/env python3
"""Oracle for the pure range/format helpers of tools/read_extract.py ported to C.

Reads the JSON cases fixture (argv[1]), recomputes each case from LIVE
tools/read_extract.py, and emits one JSON object per case.
"""
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.getcwd())
sys.path.insert(0, "/home/wubu/.hermes/hermes-agent")

from tools import read_extract as m  # noqa: E402


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op")
        arg = c.get("arg", {})
        if op == "group_ranges":
            pages = arg.get("pages", [])
            runs = m._group_ranges(pages)
            print(json.dumps({"op": "group_ranges", "runs": runs}))
        elif op == "page_ranges":
            pages = arg.get("pages", [])
            out = m._page_ranges(pages)
            print(json.dumps({"op": "page_ranges", "out": out}))
        elif op == "gap_map":
            counts = arg.get("counts", [])
            texts = arg.get("texts", [])
            empty = arg.get("empty", [])
            out = m._gap_map(counts, texts, empty)
            print(json.dumps({"op": "gap_map", "out": out}))
        elif op == "human_size":
            nb = arg if isinstance(arg, (int, float)) else 0
            out = m._human_size(int(nb))
            print(json.dumps({"op": "human_size", "out": out}))
        elif op == "base64_bytes":
            payload = arg if isinstance(arg, str) else ""
            out = m._base64_bytes(payload)
            print(json.dumps({"op": "base64_bytes", "out": out}))
        elif op == "clean_stream":
            out = m._clean_stream_text(arg if isinstance(arg, str) else "")
            print(json.dumps({"op": "clean_stream", "out": out}))
        elif op == "nb_output_text":
            if isinstance(arg, str) and arg:
                try:
                    arg = json.loads(arg)
                except Exception:
                    arg = {}
            out = m._notebook_output_text(arg if isinstance(arg, dict) else {})
            print(json.dumps({"op": "nb_output_text", "out": out}))
        elif op == "nb_outputs":
            if isinstance(arg, str) and arg:
                try:
                    arg = json.loads(arg)
                except Exception:
                    arg = {}
            arg = arg if isinstance(arg, dict) else {}
            cell = arg.get("cell", {})
            out = m._notebook_outputs(cell, arg.get("jq", ""), arg.get("filename", ""))
            print(json.dumps({"op": "nb_outputs", "out": out}))
        else:
            print(json.dumps({"op": "unknown"}))


if __name__ == "__main__":
    main()
