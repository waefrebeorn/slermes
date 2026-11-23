#!/usr/bin/env python3
"""Oracle for the utils/files.py C port (gate w5_files_19ae74).

Constants (the YAML-1.1 ambiguous-word set, the Windows contended-rename errno
set, and the racy-mtime window) plus file_signature() are AST-extracted from
the LIVE quarry module. Int64 values are compared as decimal strings.
"""
import ast
import json
import sys
import types
from pathlib import Path

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "utils" / "files.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "utils" / "files.py")
sys.path.insert(0, str(QUARRY))

# parent-package bootstrap for the LIVE utils package
utils = sys.modules.get("utils")
if utils is None:
    utils = types.ModuleType("utils")
    utils.__path__ = [str(QUARRY / "utils")]
    sys.modules["utils"] = utils

tree = ast.parse(Path(SRC).read_text(encoding="utf-8"))
ns = {"__name__": "w5_files_live", "__builtins__": __builtins__}
WANT_CONST = {"_YAML11_AMBIGUOUS_WORDS", "_WINDOWS_CONTENDED_REPLACE_ERRORS",
              "RACY_MTIME_WINDOW_NS"}
for n in tree.body:
    if isinstance(n, ast.Assign):
        tg = n.targets[0]
        if isinstance(tg, ast.Name) and tg.id in WANT_CONST:
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    elif isinstance(n, ast.FunctionDef) and n.name == "file_signature":
        exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)

AMBIG = ns["_YAML11_AMBIGUOUS_WORDS"]
CONTENDED = ns["_WINDOWS_CONTENDED_REPLACE_ERRORS"]
WINDOW = ns["RACY_MTIME_WINDOW_NS"]
FILE_SIGNATURE = ns["file_signature"]


def main():
    n = mism = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        func = rec.get("func")
        got = rec.get("out")
        try:
            if func == "w5_files_19ae74_yaml11_ambiguous":
                exp = 1 if rec.get("word", "").lower() in AMBIG else 0
            elif func == "w5_files_19ae74_contended_replace_error":
                we = rec.get("winerror", -1)
                we = None if we == -1 else we
                exp = 1 if (bool(rec.get("is_windows", False)) and we in CONTENDED) else 0
            elif func == "w5_files_19ae74_mtime_is_racy":
                mt = int(rec.get("mtime_ns", "0"))
                now = int(rec.get("now_ns", "0"))
                exp = 1 if (now - mt < WINDOW) else 0
            elif func == "w5_files_19ae74_file_signature":
                st = types.SimpleNamespace(
                    st_mtime_ns=int(rec.get("mtime_ns", "0")),
                    st_size=int(rec.get("size", "0")),
                    st_ino=int(rec.get("ino", "0")),
                    st_ctime_ns=int(rec.get("ctime_ns", "0")),
                )
                exp = [str(x) for x in FILE_SIGNATURE(st)]
            else:
                exp = {"__unknown__": func}
        except BaseException as e:
            exp = {"__raise__": type(e).__name__}
        n += 1
        if got != exp:
            mism += 1
            if shown < 10:
                shown += 1
                print("MISMATCH %s case %d:\n  C=%s\n  P=%s" % (
                    func, n, json.dumps(got), json.dumps(exp)), file=sys.stderr)
    print("ORACLE w5_files_19ae74: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
