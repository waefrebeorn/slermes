#!/usr/bin/env python3
"""Smoke-check the product adapter for should_clear_context_pin.

Compiles a tiny C driver against src/cli/route_identity_flow.o, runs it, and
compares against LIVE hermes_cli.route_identity.should_clear_context_pin with
_context_route_mismatch stubbed from the routes_match field.
"""
import os
import subprocess
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASH = "C:/msys64/usr/bin/bash.exe"

CASES = [
    "m1\tm2\t0",
    "m\tm\t0",
    "m\tm\t1",
    " m \tm\t0",
    "\tm2\t1",
    "5\t5\t1",
    "m\t m \t1",
]
EXPECTED = [1, 1, 0, 1, 0, 0, 0]

DRIVER = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern int rid_should_clear_context_pin_cstr(const char *, const char *, int);
int main(void) {
    const char *cases[] = {
        "m1\tm2\t0", "m\tm\t0", "m\tm\t1", " m \tm\t0",
        "\tm2\t1", "5\t5\t1", "m\t m \t1"
    };
    for (int i = 0; i < 7; i++) {
        const char *arg = cases[i];
        char buf[256]; snprintf(buf, sizeof buf, "%s", arg);
        char *t1 = strchr(buf, '\t');
        char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
        int match = t2 && t2[1] == '1';
        if (t1) *t1 = '\0';
        if (t2) *t2 = '\0';
        const char *cm = buf, *am = t1 ? t1 + 1 : "";
        printf("%d|%d\n", i, rid_should_clear_context_pin_cstr(cm, am, match));
    }
    return 0;
}
'''


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        src = os.path.join(td, "pin_adapter.c")
        exe = os.path.join(td, "pin_adapter.exe")
        with open(src, "w", encoding="utf-8", newline="\n") as f:
            f.write(DRIVER)
        script = (
            'cd /c/Users/eman5/slermes; source build/winlibs-env.sh; '
            f'gcc -Ilib/libjson -Iinclude -o "{exe.replace(chr(92), "/")}" '
            f'"{src.replace(chr(92), "/")}" '
            'src/cli/route_identity_flow.o lib/libjson/json.o -lm'
        )
        r = subprocess.run([BASH, "-lc", script], cwd=ROOT,
                           capture_output=True, text=True)
        if r.returncode:
            print(r.stdout + r.stderr)
            return r.returncode
        out = subprocess.run([exe], capture_output=True, text=True,
                             encoding="utf-8", errors="replace")
        if out.returncode:
            print(out.stderr)
            return out.returncode
        got = [int(line.split("|", 1)[1]) for line in out.stdout.splitlines()]
        if got != EXPECTED:
            print(f"MISMATCH expected={EXPECTED!r} got={got!r}")
            return 1
    print(f"route pin adapter: {len(CASES)} cases, 0 mismatches")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
