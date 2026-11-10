#!/usr/bin/env python3
"""Oracle for port_hosted_rooms_common.c -- drives the LIVE
gateway/hosted_rooms_common.py defs. The sqlite / clock half is not imported.
"""
import ast
import json
import re
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\gateway\hosted_rooms_common.py"
WANTED = ["identifier", "bounded_int", "exact_fields", "text", "compact_json",
          "canonical_json", "utf8_len"]


class RoomError(Exception):
    pass


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {"re": re, "json": json, "Any": object, "Mapping": dict,
          "HostedRoomError": RoomError}
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


def call(fn, *args, **kw):
    """Run the live fn, returning (ok, value_or_message)."""
    try:
        return True, fn(*args, **kw)
    except Exception as e:
        return False, str(e)


def main():
    ns = extract()
    cases = 0
    mism = 0

    idents = {
        "simple": (2, "abc", 128, 1), "padded": (2, "  abc  ", 128, 1),
        "empty": (2, "   ", 128, 1), "too-long": (2, "abcdefghij", 5, 1),
        "bad-shape": (2, "1abc", 128, 1), "bad-start": (2, "-abc", 128, 1),
        "space-mid": (2, "a b", 128, 1), "colon": (2, "a:b", 128, 1),
        "no-pattern": (2, "a b!", 128, 0), "not-str": (0, None, 128, 1),
        "unicode": (2, "café", 128, 1),
    }
    fields = {
        "exact": (["a", "b"], ["a", "b"], []),
        "missing": (["a"], ["a", "b", "c"], []),
        "unknown": (["a", "b", "z", "y"], ["a", "b"], []),
        "optional": (["a", "b", "opt"], ["a", "b"], ["opt"]),
        "empty-obj": ([], ["a", "b"], []),
    }
    texts = {
        "simple": (2, "hello", 100, 1), "padded": (2, "  hi  ", 100, 1),
        "blank": (2, "   ", 100, 1), "too-large": (2, "abcdef", 3, 1),
        "unicode": (2, "café", 5, 1), "unicode-4": (2, "café", 4, 1),
        "no-strip": (2, "  hi  ", 10, 0), "not-str": (0, None, 100, 1),
        "exact-fit": (2, "abc", 3, 1),
    }
    compacts = {
        "object": '{"b":1,"a":2}', "nested": '{"z":{"d":4,"c":3},"a":[1,2]}',
        "string": '"hi"', "array": "[3,1,2]",
        "escapes": '{"s":"a\\nb\\t\\"c\\""}', "numbers": '{"i":42,"f":1.5}',
        "empty": "{}", "null": None,
    }
    utf8s = {
        "ascii": ["abc", "de"], "unicode": ["café", "!"], "none": [],
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
            if fn == "identifier":
                kind, val, mc, up = idents[label]
                # the Python takes the raw value; the C harness's `kind` 2 means str
                want = call(ns["identifier"], val, label="name", error=RoomError,
                            max_chars=mc,
                            **({} if up else {"pattern": None}))
                if want[0]:
                    if c.get("rc") != 0 or c.get("out") != want[1]:
                        mism += 1
                        print(f"MISMATCH identifier case={label} C=(rc={c.get('rc')},out={c.get('out')!r}) PY={want!r}")
                else:
                    if c.get("rc") != -1 or c.get("err") != want[1]:
                        mism += 1
                        print(f"MISMATCH identifier case={label} C=(rc={c.get('rc')},err={c.get('err')!r}) PY=({want[1]!r})")

            elif fn == "exact_fields":
                keys, req, opt = fields[label]
                want = call(ns["exact_fields"], {k: 1 for k in keys}, label="room",
                            required=set(req), optional=set(opt), error=RoomError)
                if want[0]:
                    if c.get("rc") != 0:
                        mism += 1
                        print(f"MISMATCH exact_fields case={label} C rc={c.get('rc')} PY ok")
                else:
                    if c.get("rc") != -1 or c.get("err") != want[1]:
                        mism += 1
                        print(f"MISMATCH exact_fields case={label} C={c.get('err')!r} PY={want[1]!r}")

            elif fn == "text":
                kind, val, mb, strip = texts[label]
                want = call(ns["text"], val, error=RoomError, label="title",
                            max_bytes=mb, strip=strip)
                if want[0]:
                    if c.get("rc") != 0 or c.get("out") != want[1]:
                        mism += 1
                        print(f"MISMATCH text case={label} C=(rc={c.get('rc')},out={c.get('out')!r}) PY={want!r}")
                else:
                    if c.get("rc") != -1 or c.get("err") != want[1]:
                        mism += 1
                        print(f"MISMATCH text case={label} C=(rc={c.get('rc')},err={c.get('err')!r}) PY=({want[1]!r})")

            elif fn == "compact_json":
                jt = compacts[label]
                want = ns["compact_json"](json.loads(jt)) if jt else None
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH compact_json case={label} C={got!r} PY={want!r}")

            elif fn == "utf8_len":
                want = ns["utf8_len"](*utf8s[label])
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH utf8_len case={label} C={got!r} PY={want!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE hosted_rooms_common: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
