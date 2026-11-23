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

MATCH_PAIRS = [
    ("https://openrouter.ai/v1", "openrouter.ai"),
    ("https://api.openrouter.ai/v1", "openrouter.ai"),
    ("https://openrouter.ai.:443/", "openrouter.ai"),
    ("https://evilopenrouter.ai/", "openrouter.ai"),
    ("https://nousresearch.com/", "nousresearch.com"),
    ("https://api.minimax.io/", "api.minimax.io"),
    ("https://api.minimaxi.com/", "api.minimaxi.com"),
    ("https://api.anthropic.com/", "api.anthropic.com"),
    ("", "openrouter.ai"),
]
MATCH_EXPECTED = [1, 1, 1, 0, 1, 1, 1, 1, 0]

NORMALIZE_CASES = [
    "HTTP://U:***@Example.COM:80/a/?q#f",
    "http://h:8080/x/",
    "https://H:443/x/",
    "http://h/",
    "http://h/?",
    "http://h?x",
    "http://h/#f",
    "http://[FE80::1%tESt]:1234/keys/",
    "http://[::1]:80/",
    "not a url",
    "",
    None,
    "5",
]
NORMALIZE_EXPECTED = [
    "http://U:***@example.com/a/?q",
    "http://h:8080/x",
    "https://h/x",
    "http://h",
    "http://h/?",
    "http://h?x",
    "http://h",
    "http://[fe80::1%tESt]:1234/keys",
    "http://[::1]",
    "not a url",
    "",
    "",
    "5",
]

DRIVER = r'''
#include <stdio.h>
#include <stdlib.h>
extern char *rid_base_url_hostname_cstr(const char *);
extern bool rid_base_url_host_matches_cstr(const char *, const char *);
extern char *rid_normalize_route_base_url_cstr(const char *);
int main(void) {
    const char *cases[] = {
        "https://api.anthropic.com/v1", "api.anthropic.com",
        "https://EXAMPLE.com.:443/x", "", "not a url",
        "https://[2001:db8::1]/", NULL
    };
    for (int i = 0; i < 7; i++) {
        char *h = rid_base_url_hostname_cstr(cases[i]);
        printf("H|%d|%s\n", i, h ? h : "");
        free(h);
    }
    const char *pairs[][2] = {
        {"https://openrouter.ai/v1", "openrouter.ai"},
        {"https://api.openrouter.ai/v1", "openrouter.ai"},
        {"https://openrouter.ai.:443/", "openrouter.ai"},
        {"https://evilopenrouter.ai/", "openrouter.ai"},
        {"https://nousresearch.com/", "nousresearch.com"},
        {"https://api.minimax.io/", "api.minimax.io"},
        {"https://api.minimaxi.com/", "api.minimaxi.com"},
        {"https://api.anthropic.com/", "api.anthropic.com"},
        {"", "openrouter.ai"}
    };
    for (int i = 0; i < 9; i++)
        printf("M|%d|%d\n", i,
               rid_base_url_host_matches_cstr(pairs[i][0], pairs[i][1]) ? 1 : 0);
    const char *urls[] = {
        "HTTP://U:***@Example.COM:80/a/?q#f", "http://h:8080/x/",
        "https://H:443/x/", "http://h/", "http://h/?", "http://h?x",
        "http://h/#f", "http://[FE80::1%tESt]:1234/keys/",
        "http://[::1]:80/", "not a url", "", NULL, "5"
    };
    for (int i = 0; i < 13; i++) {
        char *u = rid_normalize_route_base_url_cstr(urls[i]);
        printf("N|%d|%s\n", i, u ? u : "");
        free(u);
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
        lines = out.stdout.splitlines()
        got_h = [l.split("|", 2)[2] for l in lines if l.startswith("H|")]
        got_m = [int(l.split("|", 2)[2]) for l in lines if l.startswith("M|")]
        got_n = [l.split("|", 2)[2] for l in lines if l.startswith("N|")]
        if got_h != EXPECTED:
            print(f"HOSTNAME MISMATCH expected={EXPECTED!r} got={got_h!r}")
            return 1
        if got_m != MATCH_EXPECTED:
            print(f"MATCH MISMATCH expected={MATCH_EXPECTED!r} got={got_m!r}")
            return 1
        if got_n != NORMALIZE_EXPECTED:
            print(f"NORMALIZE MISMATCH expected={NORMALIZE_EXPECTED!r} got={got_n!r}")
            return 1
    total = len(CASES) + len(MATCH_PAIRS) + len(NORMALIZE_CASES)
    print(f"route_identity adapters: {total} cases, 0 mismatches")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
