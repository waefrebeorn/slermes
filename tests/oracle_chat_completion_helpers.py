#!/usr/bin/env python3
"""Oracle for port_chat_completion_helpers.c -- drives the LIVE
agent/chat_completion_helpers.py defs. The asyncio / SDK stream half is not
imported; the pure defs are ast-extracted together with `re`, `json` and
`contextlib`.
"""
import ast
import contextlib
import json
import re
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\agent\chat_completion_helpers.py"
WANTED = ["_status_code_from_value", "_status_code_from_payload",
          "_json_object_from_text", "_payload_has_error_shape",
          "_parse_provider_sse_events"]


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {"re": re, "json": json, "contextlib": contextlib, "Any": object}
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


def pyval(kind, ival, sval):
    if kind == 1:
        return ival
    if kind == 2:
        return sval
    return None


def main():
    ns = extract()
    cases = 0
    mism = 0

    codes = {
        "int-ok": (1, 200, None), "int-low": (1, 99, None), "int-high": (1, 600, None),
        "int-599": (1, 599, None), "bool": (0, 0, None),
        "str-plain": (2, 0, "503 Service Unavailable"),
        "str-prefix": (2, 0, "HTTP_STATUS/429 rate limited"),
        "str-lower": (2, 0, "http_status/404"),
        "str-bad": (2, 0, "no digits here"), "str-600": (2, 0, "600 too big"),
        "null": (0, 0, None),
    }
    payloads = {
        "status_code": '{"status_code":503}', "status": '{"status":"429"}',
        "http_status": '{"http_status":404}', "nested": '{"error":{"code":401}}',
        "top-code": '{"code":408}', "none": '{"message":"hi"}',
        "not-dict": None, "bad-candidate": '{"status_code":"nope"}',
    }
    objs = {
        "object": '{"a":1}', "array": "[1,2,3]", "scalar": '"str"',
        "padded": '   {"a": 1}  ', "invalid": "{not json}", "empty": "", "null": None,
    }
    shapes = {
        "error-dict": '{"error":{"message":"boom"}}', "error-str": '{"error":"boom"}',
        "msg+code": '{"message":"boom","code":42}',
        "msg+ecode": '{"message":"boom","error_code":"E1"}',
        "msg+status": '{"message":"boom","status_code":500}',
        "msg-only": '{"message":"boom"}', "empty-dict": "{}", "not-dict": None,
        "msg-empty": '{"message":"","code":42}',
    }
    sses = {
        "one-event": "event: message\ndata: hello\n\n",
        "two-data": "event: message\ndata: line1\ndata: line2\n\n",
        "no-event": "data: only data\n\n",
        "comment": ": status 200\ndata: hi\n\n",
        "fields": "data: hi\nstatus: 503\nid: 7\n\n",
        "no-colon": "data\n\n",
        "multi": "event: a\ndata: 1\n\nevent: b\ndata: 2\n\n",
        "empty": "",
        "blank-mid": "data: 1\n\ndata: 2\n\n",
        "crlf": "event: message\r\ndata: hi\r\n\r\n",
        "no-final-blank": "event: message\ndata: hi",
        "space-field": "event:  message\ndata:  padded\n\n",
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
            if fn == "_status_code_from_value":
                k, i, s = codes[label]
                want = ns["_status_code_from_value"](pyval(k, i, s))
                got = c.get("out")
                got = None if got == -1 else got
                if got != want:
                    mism += 1
                    print(f"MISMATCH _status_code_from_value case={label} C={got!r} PY={want!r}")

            elif fn == "_status_code_from_payload":
                pj = payloads[label]
                want = ns["_status_code_from_payload"](json.loads(pj) if pj else None)
                got = c.get("out")
                got = None if got == -1 else got
                if got != want:
                    mism += 1
                    print(f"MISMATCH _status_code_from_payload case={label} C={got!r} PY={want!r}")

            elif fn == "_json_object_from_text":
                text = objs[label]
                want = ns["_json_object_from_text"](text) if text is not None else ns["_json_object_from_text"](None)
                got = c.get("out")
                try:
                    got_obj = json.loads(got) if got else None
                except json.JSONDecodeError:
                    got_obj = got
                if got_obj != want:
                    mism += 1
                    print(f"MISMATCH _json_object_from_text case={label} C={got_obj!r} PY={want!r}")

            elif fn == "_payload_has_error_shape":
                pj = shapes[label]
                want = bool(ns["_payload_has_error_shape"](json.loads(pj) if pj else None))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _payload_has_error_shape case={label} C={got!r} PY={want!r}")

            elif fn == "_parse_provider_sse_events":
                want = ns["_parse_provider_sse_events"](sses[label])
                got = c.get("out") or []
                try:
                    got_obj = [json.loads(g) if isinstance(g, str) else g for g in got]
                except json.JSONDecodeError:
                    got_obj = got
                if got_obj != want:
                    mism += 1
                    print(f"MISMATCH _parse_provider_sse_events case={label}\n"
                          f"  C={json.dumps(got_obj)[:260]!r}\n  PY={json.dumps(want)[:260]!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE chat_completion_helpers: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
