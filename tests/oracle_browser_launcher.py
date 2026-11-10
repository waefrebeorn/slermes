#!/usr/bin/env python3
"""Oracle for port_browser_launcher.c -- drives the LIVE
tools/browser_launcher.py defs. The download / spawn / singleton half is not
imported; the pure defs are ast-extracted with the module constants they read.
"""
import ast
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\tools\browser_launcher.py"
WANTED = ["_has_display", "build_chromium_argv"]


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {"sys": sys, "tuple": tuple, "list": list, "str": str, "bool": bool,
          "dict": dict, "Any": object, "Optional": object}
    for node in ast.parse(src).body:
        # both plain Assign and annotated Assign (e.g.
        # `_BASE_FLAGS: tuple[str, ...] = (...)`) carry the constants
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    # the two externals build_chromium_argv closes over
    ns["_configured_chromium_args"] = lambda: list(ns["_CONFIGURED"])
    ns["needs_chromium_sandbox_bypass"] = lambda: bool(ns["_NEEDS_BYPASS"])
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    displays = {
        "x11": {"DISPLAY": ":0"}, "wayland": {"WAYLAND_DISPLAY": "wayland-0"},
        "empty": {"DISPLAY": ""}, "none": {"PATH": "/usr/bin"}, "no-env": {},
    }
    argvs = {
        "plain":      (0, 0, 1, 0, 0, [], []),
        "packaged":   (1, 0, 1, 0, 0, [], []),
        "macos":      (1, 1, 1, 0, 0, [], []),
        "headed":     (0, 0, 0, 0, 0, [], []),
        "bypass":     (0, 0, 1, 1, 1, [], []),
        "bypass-off": (0, 0, 1, 0, 1, [], []),
        "no-need":    (0, 0, 1, 1, 0, [], []),
        "configured": (0, 0, 1, 0, 0, ["--custom=1"], ["--proxy=x"]),
        "dedupe":     (0, 0, 1, 1, 1, ["--no-sandbox"], []),
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
            if fn == "_has_display":
                want = bool(ns[fn](displays[label]))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _has_display case={label} C={got!r} PY={want!r}")

            elif fn == "build_chromium_argv":
                packaged, is_macos, headed, sb, nb, configured, extra = argvs[label]
                ns["_CONFIGURED"] = configured
                ns["_NEEDS_BYPASS"] = nb
                want = ns[fn]("/chrome", "/udd", packaged=packaged, headed=headed,
                              sandbox_bypass=sb, extra_args=extra)
                if is_macos:
                    # the live fn tests sys.platform; simulate darwin for this case
                    real = sys.platform
                    sys.platform = "darwin"
                    try:
                        want = ns[fn]("/chrome", "/udd", packaged=packaged, headed=headed,
                                      sandbox_bypass=sb, extra_args=extra)
                    finally:
                        sys.platform = real
                got = c.get("out") or []
                if got != want:
                    mism += 1
                    print(f"MISMATCH build_chromium_argv case={label}\n"
                          f"  C={got!r}\n  PY={want!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE browser_launcher: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
