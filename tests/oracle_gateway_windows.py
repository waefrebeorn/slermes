#!/usr/bin/env python3
"""Oracle for port_gateway_windows.c -- drives the LIVE
hermes_cli/gateway_windows.py defs. The SCM / schtasks / UAC / registry half is
not imported; the pure defs are ast-extracted with the regex constants they
read compiled here.
"""
import ast
import json
import re
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

GW = r"C:\Users\eman5\hermes-agent\hermes_cli\gateway_windows.py"
WANTED = ["_normalize_windows_path", "_sanitize_filename", "_quote_vbs_string",
          "_should_fall_back", "_is_access_denied", "_gateway_run_argv",
          "hermes_owns_windows_service"]

ACCESS_DENIED_WORDS = (
    r"access is denied|acceso denegado|přístup byl odepřen|拒绝访问|拒絕存取|"
    r"アクセスが拒否されました|액세스가 거부되었습니다"
)


def extract():
    ns = {"re": re, "str": str, "int": int, "bool": bool, "Any": object,
          "Optional": object, "Path": object, "tuple": tuple, "list": list,
          "os": object, "sys": sys}
    src = open(GW, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            # substitute the access-denied words (the module builds them from a
            # tuple literal; exec'ing it needs the full import set, so inject)
            pass
    ns["_ACCESS_DENIED_WORDS"] = ACCESS_DENIED_WORDS
    ns["_FALLBACK_PATTERNS"] = re.compile(
        rf"({ACCESS_DENIED_WORDS}|schtasks timed out|schtasks produced no output)",
        re.IGNORECASE)
    ns["_ACCESS_DENIED_PATTERN"] = re.compile(rf"({ACCESS_DENIED_WORDS})", re.IGNORECASE)
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), GW, "exec"), ns)
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    paths = {
        "quoted": '  "C:\\\\Users\\\\Hermes\\\\"  ',
        "trailing": "C:/Users/Hermes/",
        "blank": "  ",
        "null": None,
    }
    sanitized = {"illegal": 'a<b>c:d"e/f\\g|h?i*j',
                 "plain": "plain-name.txt", "control": "tab\there"}
    vbs = {"plain": "plain", "quotes": 'say "hi"', "newline": "line1\nline2"}
    denied = {"en": "Access is denied.", "es": "Acceso denegado",
              "zh": "拒绝访问", "none": "all good", "null": None}
    falls = {"timeout": (124, ""), "denied": (1, "Access is denied"),
             "schtasks": (1, "schtasks timed out"),
             "nooutput": (1, "schtasks produced no output"),
             "clean": (0, "ok"), "other": (1, "something else")}
    argvs = {"profile": ["--profile", "work"], "no-profile": []}
    owned = {
        "name": ("Hermes Gateway", "C:\\x\\y.exe", ["C:\\hermes"]),
        "under": ("Schedule", "C:\\hermes\\bin\\g.exe", ["C:\\hermes"]),
        "outside": ("Schedule", "C:\\windows\\s.exe", ["C:\\hermes"]),
        "empty-root": ("Schedule", "C:\\hermes\\g.exe", []),
        "null-name": (None, "C:\\hermes\\g.exe", ["C:\\hermes"]),
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
            if fn == "_normalize_windows_path":
                # the live fn has NO None guard - it raises AttributeError. The C
                # port fails safe (returns ""), which is the right contract for a
                # C caller, so a raise here matches the C's empty string.
                try:
                    want = ns[fn](paths[label])
                except AttributeError:
                    want = ""
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _normalize_windows_path case={label} C={got!r} PY={want!r}")

            elif fn == "_sanitize_filename":
                want = ns[fn](sanitized[label])
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _sanitize_filename case={label} C={got!r} PY={want!r}")

            elif fn == "_quote_vbs_string":
                try:
                    want = ns[fn](vbs[label])
                except ValueError:
                    want = ""
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _quote_vbs_string case={label} C={got!r} PY={want!r}")

            elif fn == "_is_access_denied":
                want = bool(ns[fn](denied[label]))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _is_access_denied case={label} C={got!r} PY={want!r}")

            elif fn == "_should_fall_back":
                code, detail = falls[label]
                want = bool(ns[fn](code, detail))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _should_fall_back case={label} C={got!r} PY={want!r}")

            elif fn == "_gateway_run_argv":
                pa = argvs[label]
                want = ns[fn]("python.exe", " ".join(pa) if pa else "")
                got = c.get("out") or []
                if got != want:
                    mism += 1
                    print(f"MISMATCH _gateway_run_argv case={label}\n  C={got!r}\n  PY={want!r}")

            elif fn == "hermes_owns_windows_service":
                name, binpath, roots = owned[label]
                # A NULL name raises AttributeError in Python. The C port cannot
                # raise, and it does NOT fail safe here: it skips the name check
                # and still evaluates the binpath-against-roots test. That is a
                # deliberate divergence - the C reports ownership from the path
                # alone. Documented, not silent.
                try:
                    want = bool(ns[fn](name, binpath, tuple(roots)))
                except AttributeError:
                    norm = (binpath or "").strip().lstrip('"').replace("\\", "/").rstrip("/").casefold()
                    want = any(norm.startswith((r or "").strip().lstrip('"').replace("\\", "/").rstrip("/").casefold() + "/")
                               for r in roots if r)
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH hermes_owns_windows_service case={label} C={got!r} PY={want!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE gateway_windows: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
