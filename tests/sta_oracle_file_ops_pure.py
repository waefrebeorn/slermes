import os
#!/usr/bin/env python3
"""
sta_oracle_file_ops_pure.py — Python oracle for the PURE, deterministic
helpers ported from tools/file_operations.py (file_text_ops.c,
file_pagination_ops.c, file_lint.c).

Imports the REAL tools.file_operations module and exercises the genuine
functions. Output contract matches tests/t_port_file_ops_pure.c: one JSON
object per line, sorted keys, ensure_ascii=False, compact separators.

For linters the C side returns {"valid":bool,"error":str}; YAML/TOML/Python
shell out to python3 (identical to this oracle), JSON uses the project's own
parser (classification matches for standard valid/invalid inputs).
"""

import json
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from hermes_cli.config import _coerce_int
from tools.file_operations_search import _pattern_has_regex_newline
from tools.file_operations_lint import (
    _lint_json_inproc, _lint_yaml_inproc, _lint_toml_inproc, _lint_python_inproc,
)
from tools.file_operations import (  # noqa: E402
    _strip_terminal_fence_leaks,
    _detect_line_ending,
    _normalize_line_endings,
    _strip_bom,
    _has_bom,
    normalize_read_pagination,
    normalize_search_pagination,
)

# read_pagination default limit mirrors the C caller (get_max_lines() -> 2000)
DEFAULT_MAX_LINES = 2000
DEFAULT_SEARCH_LIMIT = 50


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def dec(s):
    """Decode fixture tokens into real characters (mirror of C harness)."""
    if s is None:
        return ""
    s = s.replace("@LF@", "\n").replace("@CRLF@", "\r\n")
    s = s.replace("@BSBSN@", "\\\\n").replace("@BSN@", "\\n")
    s = s.replace("@BOM@", "\ufeff")
    return s


def split_kv(line):
    line = line.rstrip("\n")
    if not line.strip() or line.startswith("#"):
        return None
    op, _, rest = line.partition(" ")
    return op, rest


def lint_result(ok_msg):
    ok, msg = ok_msg
    return {"valid": bool(ok), "error": msg}


def main():
    # The runner feeds C output on stdin; read the shared fixture instead so
    # this oracle stays a reference emitter, not a stdin validator.
    with open(sys.argv[1], encoding="utf-8") as f:
      for raw in f:
        parsed = split_kv(raw)
        if parsed is None:
            continue
        op, rest = parsed
        v = dec(rest) if rest else ""

        if op == "fence":
            emit({"op": "fence", "in": v,
                  "out": _strip_terminal_fence_leaks(v)})

        elif op == "detect_le":
            le = _detect_line_ending(v)
            out = {"\r\n": "crlf", "\n": "lf"}.get(le, "unknown")
            emit({"op": "detect_le", "in": v, "out": out})

        elif op == "norm_le":
            if "|" in v:
                target, _, text = v.partition("|")
            else:
                target, text = "\n", ""
            emit({"op": "norm_le", "target": target, "text": text,
                  "out": _normalize_line_endings(text, target)})

        elif op == "strip_bom":
            out, had = _strip_bom(v)
            emit({"op": "strip_bom", "in": v, "out": out, "had_bom": bool(had)})

        elif op == "pat_newline":
            emit({"op": "pat_newline", "pattern": v,
                  "has": bool(_pattern_has_regex_newline(v))})

        elif op == "norm_read":
            # C harness seeds sscanf defaults; a failed conversion leaves
            # that field AND all later fields at their seed values.
            parts = rest.split("|")
            dl, off, lim = DEFAULT_MAX_LINES, 1, 500
            for idx, (target, raw) in enumerate(zip(("dl", "off", "lim"), parts)):
                try:
                    val = int(raw)
                except (TypeError, ValueError):
                    break
                if target == "dl": dl = val
                elif target == "off": off = val
                else: lim = val
            dl = dl or DEFAULT_MAX_LINES
            noff, nlim = normalize_read_pagination(off, lim)
            emit({"op": "norm_read", "default_limit": dl, "offset": off,
                  "limit": lim, "out": json.dumps(
                      {"offset": noff, "limit": nlim}, separators=(",", ":"))})

        elif op == "norm_search":
            parts = rest.split("|")
            dl, off, lim = DEFAULT_SEARCH_LIMIT, 0, 50
            for target, raw in zip(("dl", "off", "lim"), parts):
                try:
                    val = int(raw)
                except (TypeError, ValueError):
                    break
                if target == "dl": dl = val
                elif target == "off": off = val
                else: lim = val
            dl = dl or DEFAULT_SEARCH_LIMIT
            noff, nlim = normalize_search_pagination(off, lim)
            emit({"op": "norm_search", "default_limit": dl, "offset": off,
                  "limit": lim, "out": json.dumps(
                      {"offset": noff, "limit": nlim}, separators=(",", ":"))})

        elif op == "lint":
            if "|" in v:
                kind, _, content = v.partition("|")
            else:
                kind, content = v, ""
            if kind == "json":
                res = lint_result(_lint_json_inproc(content))
            elif kind == "yaml":
                res = lint_result(_lint_yaml_inproc(content))
            elif kind == "toml":
                res = lint_result(_lint_toml_inproc(content))
            elif kind == "python":
                res = lint_result(_lint_python_inproc(content))
            else:
                res = {"valid": False, "error": "unknown kind"}
            emit({"op": "lint", "kind": kind, "content": content,
                  "result": json.dumps(
                      {"valid": bool(res.get("valid")),
                       "err": 1 if str(res.get("error") or "").strip() else 0},
                      separators=(",", ":"))})

        else:
            emit({"op": "unknown", "raw": raw.rstrip("\n")})

    return 0


if __name__ == "__main__":
    sys.exit(main())
