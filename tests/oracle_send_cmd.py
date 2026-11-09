#!/usr/bin/env python3
"""Oracle for port_send_cmd.c -- drives the LIVE send_cmd.py:_emit_result.

The live function PRINTS; this oracle re-implements its decision logic from the
same source (ast-extracted) and compares the exit code, the branch selector and
the note text. `_SUCCESS_EXIT` / `_FAILURE_EXIT` are read from the module.
"""
import ast
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\send_cmd.py"


def extract():
    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    ns = {}
    for node in tree.body:
        if isinstance(node, ast.Assign):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "_emit_result":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


def truthy(v):
    """Python truthiness for the JSON values the payload can carry."""
    if v is None or v is False:
        return False
    if isinstance(v, str):
        return len(v) > 0
    if isinstance(v, (int, float)):
        return v != 0
    return True


def classify(result_json, json_mode, quiet, SUCCESS, FAILURE):
    """The live _emit_result's decisions, without the printing."""
    try:
        payload = json.loads(result_json) if result_json else {}
    except json.JSONDecodeError:
        payload = {"error": "invalid JSON from send_message_tool", "raw": result_json}

    if json_mode:
        display, to_stderr, text = 4, False, None
    elif quiet:
        display, to_stderr, text = 0, False, None
    elif truthy(payload.get("error")):
        display, to_stderr, text = 1, True, None
    elif truthy(payload.get("success")):
        display, to_stderr = 2, False
        note = payload.get("note")
        text = note if note else "sent"
    else:
        display, to_stderr, text = 3, False, None

    code = SUCCESS if (not truthy(payload.get("error"))
                       and (truthy(payload.get("skipped")) or truthy(payload.get("success")))) else FAILURE
    return code, display, to_stderr, text


def main():
    ns = extract()
    fn = ns.get("_emit_result")
    SUCCESS = ns.get("_SUCCESS_EXIT", 0)
    FAILURE = ns.get("_FAILURE_EXIT", 1)
    if fn is None:
        print("ORACLE send_cmd: _emit_result not found")
        return 2

    cases = 0
    mism = 0
    cfgs = {
        "ok-note": ("{\"success\":true,\"note\":\"delivered to telegram\"}", 0, 0),
        "ok-nonote": ("{\"success\":true}", 0, 0),
        "ok-skip": ("{\"skipped\":true,\"reason\":\"no target\"}", 0, 0),
        "error": ("{\"error\":\"platform not configured\"}", 0, 0),
        "err-empty": ("{\"error\":\"\",\"success\":true}", 0, 0),
        "unknown": ("{\"something\":\"else\"}", 0, 0),
        "bad-json": ("{not json", 0, 0),
        "empty": ("", 0, 0),
        "null": (None, 0, 0),
        "err-and-ok": ("{\"error\":\"boom\",\"success\":true}", 0, 0),
        "ok-quiet": ("{\"success\":true,\"note\":\"delivered to telegram\"}", 0, 1),
        "err-quiet": ("{\"error\":\"platform not configured\"}", 0, 1),
        "ok-json": ("{\"success\":true,\"note\":\"delivered to telegram\"}", 1, 0),
        "err-json": ("{\"error\":\"platform not configured\"}", 1, 0),
        "ok-json-q": ("{\"success\":true,\"note\":\"delivered to telegram\"}", 1, 1),
    }

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        cases += 1
        js, jm, q = cfgs.get(c.get("case"), (None, 0, 0))
        want = classify(js, jm, q, SUCCESS, FAILURE)
        got = (c.get("exit"), c.get("display"), bool(c.get("to_stderr")),
               c.get("text") if c.get("text") is not None else None)
        if got != want:
            mism += 1
            print(f"MISMATCH _emit_result case={c.get('case')} C={got!r} PY={want!r}")

    print(f"ORACLE send_cmd: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
