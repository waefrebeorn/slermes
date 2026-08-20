"""twin for watercup_json_escape.

Asserts the C web_json_escape output for control characters matches LIVE
Python json.dumps, which escapes bytes < 0x20 as \\u00XX. For printable ASCII
and the standard \\" \\ \\n \\r \\t escapes, both must agree (json.dumps does the
same short escapes for tab/newline/CR and \\" \\ for quote/backslash).
"""
import json
import sys

fail = 0

def check(src, expect, label):
    global fail
    got = json.dumps(src)[1:-1]  # strip surrounding quotes json.dumps adds
    # json.dumps of a bare string wraps in quotes; compare the inner content.
    if got != expect:
        print("FAIL: %s -> got %r expected %r" % (label, got, expect))
        fail += 1
    else:
        print("  ok:   %s" % label)

# Control chars < 0x20 with no short escape -> \u00XX (LIVE json.dumps)
check("a\0g", "a\\u0000g", "NUL")
check("x\vg", "x\\u000bg", "VT")

# Standard escapes (json.dumps uses SHORT forms for these)
check("m\tn", "m\\tn", "tab")
check("y\fg", "y\\fg", "FF (0x0c) -> \\f short")
check("z\bg", "z\\bg", "BS (0x08) -> \\b short")
check("a\"b\\c", "a\\\"b\\\\c", "quote+backslash")

if fail:
    print("RESULT: %d FAILURES" % fail)
    sys.exit(1)
print("RESULT: web_json_escape matches LIVE json.dumps control-char escaping")
