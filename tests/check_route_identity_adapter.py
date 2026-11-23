#!/usr/bin/env python3
"""Smoke-check the product adapter for utils.base_url_hostname.

Compiles a tiny C driver against src/cli/route_identity_flow.o, runs it, and
compares every result to the LIVE Python values captured from hermes-agent.
This is a product-wiring check: the function is reachable from a production
caller (src/agent/agent_runtime_pure.c), not just from a harness.
"""
import os
import subprocess
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASH = "C:/msys64/usr/bin/bash.exe"

CASES = [
    "https://api.anthropic.com/v1",
    "api.anthropic.com",
    "https://EXAMPLE.com.:443/x",
    "",
    "not a url",
    "https://[2001:db8::1]/",
    None,
]
EXPECTED = [
    "api.anthropic.com",
    "api.anthropic.com",
    "example.com",
    "",
    "not a url",
    "2001:db8::1",
    "",
]

DRIVER = r'''
#include <stdio.h>
#include <stdlib.h>
extern char *rid_base_url_hostname_cstr(const char *);
int main(void) {
    const char *cases[] = {
        "https://api.anthropic.com/v1", "api.anthropic.com",
        "https://EXAMPLE.com.:443/x", "", "not a url",
        "https://[2001:db8::1]/", NULL
    };
    for (int i = 0; i < 7; i++) {
        char *h = rid_base_url_hostname_cstr(cases[i]);
        printf("%d|%s\n", i, h ? h : "");
        free(h);
    }
    return 0;
}
'''


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        src = os.path.join(td, "adapter.c")
        exe = os.path.join(td, "adapter.exe")
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
        got = [line.split("|", 1)[1] for line in out.stdout.splitlines()]
        if got != EXPECTED:
            print(f"MISMATCH expected={EXPECTED!r} got={got!r}")
            return 1
    print(f"route_identity adapter: {len(CASES)} cases, 0 mismatches")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
