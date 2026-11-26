#!/usr/bin/env python3
"""Self-test for scripts/scan_json_freed_use.py.

The scanner is a heuristic, so it gets fixtures with known answers. Four shapes that actually occurred in
this tree, plus two that must stay quiet -- a scanner that fires on working code gets ignored, and one
that stays quiet on broken code is worse than nothing.

    python tests/unit/scan_json_freed_use_selftest.py     -> prints "scanner selftest: PASS"
"""
from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SCANNER = REPO / "scripts" / "scan_json_freed_use.py"

# ---- expect a finding -------------------------------------------------------------------------------
BUG_DIRECT = """
char *h(const char *args_json) {
    json_node_t *args = json_parse(args_json, NULL);
    const char *query = json_object_get_string(args, "query", NULL);
    json_free(args);
    return search(query);
}
"""

BUG_INSIDE_BLOCK = """
char *h(const char *args_json) {
    json_node_t *args = json_parse(args_json, NULL);
    const char *query = json_get_str(args, "query", NULL);
    if (args) {
        json_free(args);
    }
    return search(query);
}
"""

BUG_TWO_PATHS = """
char *h(const char *args_json) {
    json_node_t *args = json_parse(args_json, NULL);
    const char *url = json_get_str(args, "url", NULL);
    char *copy = url ? strdup(url) : NULL;
    json_free(args);
    return run(copy);
}
"""

# ---- must stay quiet ---------------------------------------------------------------------------------
OK_EARLY_RETURN = """
char *h(const char *args_json) {
    json_node_t *args = json_parse(args_json, NULL);
    const char *url = json_get_str(args, "url", NULL);
    if (!url) { json_free(args); return strdup("missing"); }
    const char *secret = url_has_secret(url);
    if (secret) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s", secret);
        json_free(args);
        return strdup(buf);
    }
    char *own = strdup(url);
    json_free(args);
    return fetch(own);
}
"""

OK_STRING_LITERAL = """
char *h(const char *args_json) {
    json_node_t *args = json_parse(args_json, NULL);
    const char *path = json_get_str(args, "path", NULL);
    json_free(args);
    json_object_set(result, "error", json_new_string("cannot write to path"));
    return finish();
}
"""

CASES = [
    ("bug_direct.c", BUG_DIRECT, 1),
    ("bug_inside_block.c", BUG_INSIDE_BLOCK, 1),
    ("ok_copied_first.c", BUG_TWO_PATHS, 0),
    ("ok_early_return.c", OK_EARLY_RETURN, 0),
    ("ok_string_literal.c", OK_STRING_LITERAL, 0),
]


def main() -> int:
    failures = []
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        for name, body, _ in CASES:
            (root / name).write_text(body, encoding="utf-8")
        out = subprocess.run([sys.executable, str(SCANNER), str(root)],
                             capture_output=True, text=True)
        text = out.stdout + out.stderr
        for name, _, expected in CASES:
            hits = text.count(name + ":")
            if hits != expected:
                failures.append(f"{name}: expected {expected} finding(s), got {hits}")

    if failures:
        for f in failures:
            print("  FAIL " + f)
        print("scanner selftest: FAIL")
        return 1
    print(f"scanner selftest: PASS ({len(CASES)} fixtures)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
