#!/usr/bin/env python3
"""sta_oracle_web_server_pure.py — Python oracle for the pure helpers in
port_web_server_schema_path.c.

We extract ONLY the pure-logic helpers from hermes_cli/web_server.py:

   _infer_type            (returns "boolean"|"number"|"string"|"list"|"object")
   _path_text             (str-or-None + .strip() + NUL reject)
   _path_is_under         (parent check)
   _fs_mime_type          (suffix table + mimetypes fallback)
   _fs_looks_binary       (NUL or >12% control bytes)
   _audio_extension_for_mime (normalize + table lookup)

Then we mirror exactly the C harness's output format: one JSON envelope per
fixture line, with key order {"op","arg","result"}. The runner diffs the two.
We are *not* re-running the C port — this is the LIVE Python reference.

`Path`, `os`, `stat`, `base64`, and `mimetypes` come from the stdlib, so we
load them in an isolated namespace; the only field we use from the hermes
module's namespace are the constants `_FS_MIME_TYPES` and
`_AUDIO_MIME_EXTENSIONS` (which we exec out of the source slice).
"""
import json
import mimetypes
import os
import stat as _stat_mod
import sys
from pathlib import Path

_HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(_HERE, ".."))   # slermes root (tests/ is one down)
# hermes_cli/web_server.py lives one level up from slermes/.
PY_ROOT = os.path.abspath(os.path.join(ROOT, ".."))

# Read web_server.py and exec out ONLY the constants + the pure helpers.
# We avoid importing the full module because FastAPI/httpx are heavy and
# because importing side-effects run module-level CONFIG_HEADER population.
WSPY = os.path.join(PY_ROOT, "hermes_cli", "web_server.py")

with open(WSPY, "r", encoding="utf-8") as _f:
    _source = _f.read()

# Pull out: (1) the `_FS_MIME_TYPES = { ... }` dict literal, (2) the
# `_AUDIO_MIME_EXTENSIONS = { ... }` dict literal, and (3) the definitions
# of `_infer_type`, `_path_text`, `_path_is_under`, `_fs_mime_type`,
# `_fs_looks_binary`, `_fs_regular_file`, and `_audio_extension_for_mime`.

import re as _re

def _grab_assign(name, src):
    """Grab `name[: type] = { ... }` assignment (possibly multi-line). Uses a
    simple brace-balance scan so multi-line dicts work. Tolerates the PEP-484
    `name: Type = {...}` annotated form used by _AUDIO_MIME_EXTENSIONS."""
    m = _re.search(rf'^{name}\s*(?::\s*[^=]+)?\s*=\s*\{{', src, _re.MULTILINE)
    if not m:
        raise RuntimeError(f"could not find {name} in {WSPY}")
    start = m.start()
    i = m.end() - 1   # position of the opening '{'
    depth = 0
    end_pos = -1
    while i < len(src):
        c = src[i]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                end_pos = i + 1
                break
        i += 1
    if end_pos < 0:
        raise RuntimeError(f"{name} dict never closes in {WSPY}")
    return src[start:end_pos]

def _grab_def(name, src):
    """Grab a `def name(...): ... ` block by counting 4-space indentation."""
    m = _re.search(rf'^def {name}\b.*?:\n', src, _re.MULTILINE)
    if not m:
        raise RuntimeError(f"could not find {name} in {WSPY}")
    start = m.start()
    # find end: first subsequent line at indentation level 0 that isn't blank
    end = start
    line_start = m.end()
    while True:
        nl = src.find('\n', line_start)
        if nl < 0:
            end = len(src)
            break
        next_start = nl + 1
        # EOF?
        if next_start >= len(src):
            end = len(src)
            break
        nxt = src[next_start:next_start + 200]
        # blank line?
        if nxt[:1] in ('\n', '\r'):
            line_start = next_start
            continue
        # zero-indent line that's not a comment?
        if nxt[:1] not in (' ', '\t') and not nxt.startswith('#'):
            end = next_start
            break
        line_start = next_start
    return src[start:end]

_chunk = "\n".join([
    "from typing import Any, Dict",
    "import os, stat, mimetypes, base64, binascii",
    "from pathlib import Path",
    "from fastapi import HTTPException",
    _grab_assign("_FS_MIME_TYPES", _source),
    _grab_assign("_AUDIO_MIME_EXTENSIONS", _source),
    _grab_def("_infer_type", _source),
    _grab_def("_path_text", _source),
    _grab_def("_path_is_under", _source),
    _grab_def("_fs_mime_type", _source),
    _grab_def("_fs_looks_binary", _source),
    _grab_def("_fs_regular_file", _source),
    _grab_def("_audio_extension_for_mime", _source),
])

_ns: dict = {}
exec(_chunk, _ns)

_infer_type = _ns["_infer_type"]
_path_text_py = _ns["_path_text"]
_path_is_under_py = _ns["_path_is_under"]
_fs_mime_type = _ns["_fs_mime_type"]
_fs_looks_binary = _ns["_fs_looks_binary"]
_audio_extension_for_mime = _ns["_audio_extension_for_mime"]

# Mirror the C harness's ws_path_status_t enum → label mapping.
STATUS_LABELS = {
    0: "ok", 1: "empty", 2: "nul", 3: "parse",
    4: "not_found", 5: "is_dir", 6: "not_regular", 7: "not_readable",
}


def ws_path_text_py(raw):
    """Run Python's _path_text, mapping HTTPException → status label + token."""
    try:
        text = _path_text_py(raw)
    except Exception:
        return ("nul", "")
    # _path_text returns the stripped string (it only rejects NUL).
    # The C port has OK / HAS_NUL + empty-after-strip OK; map here:
    if "\x00" in text:
        return ("nul", text)
    return ("ok", text)


def emit(op, arg, result, lines_out):
    # Compact JSON (no whitespace) to line up byte-for-byte with the C harness.
    lines_out.append(json.dumps({"op": op, "arg": arg, "result": result},
                                separators=(",", ":")))


def run(fixture_path):
    lines_out = []
    with open(fixture_path, "r", encoding="utf-8") as fh:
        for raw_line in fh:
            # mirror C: trim leading ws, skip blank / '#'
            line = raw_line.rstrip("\r\n")
            stripped = line.lstrip(" \t")
            if not stripped or stripped.startswith("#"):
                continue
            parts = stripped.split("|", 2)
            op = parts[0]
            a = parts[1] if len(parts) > 1 else ""

            def _unescape(s):
                out = []
                i = 0
                while i < len(s):
                    if i + 1 < len(s) and s[i] == '\\':
                        c2 = s[i+1]
                        if c2 == '0':   out.append('\0'); i += 2; continue
                        if c2 == 'n':   out.append('\n'); i += 2; continue
                        if c2 == 't':   out.append('\t'); i += 2; continue
                        if c2 == 'r':   out.append('\r'); i += 2; continue
                        if c2 == '\\':  out.append('\\'); i += 2; continue
                    out.append(s[i]); i += 1
                return "".join(out)

            if op == "infer_type_bool":
                v = (a.strip() == "true")
                result = _infer_type(v)
                emit(op, a, result, lines_out)
            elif op == "infer_type_int":
                result = _infer_type(int(a.strip() or "0"))
                emit(op, a, result, lines_out)
            elif op == "infer_type_f64":
                result = _infer_type(float(a.strip() or "0.0"))
                emit(op, a, result, lines_out)
            elif op == "infer_type_str":
                result = _infer_type(a)
                emit(op, a, result, lines_out)
            elif op == "path_text":
                buf = _unescape(a)
                status, txt = ws_path_text_py(buf)
                # Strip embedded NUL from json -- Python's json would print
                # it as \u0000, match our C-side \u-escaping.
                esc = []
                for c in txt:
                    if c == "\x00":    esc.append("\\u0000")
                    elif c == "\n":    esc.append("\\n")
                    elif c == "\r":    esc.append("\\r")
                    elif c == "\t":    esc.append("\\t")
                    elif ord(c) < 0x20: esc.append(f"\\u{ord(c):04x}")
                    else:              esc.append(c)
                esc_s = "".join(esc)
                result = f"{status}|{esc_s}"
                emit(op, a, result, lines_out)
            elif op == "path_is_under":
                b = parts[2] if len(parts) > 2 else "/"
                result = "true" if _path_is_under_py(Path(a), Path(b)) else "false"
                emit(op, a, result, lines_out)
            elif op == "fs_mime":
                result = _fs_mime_type(Path(a))
                emit(op, a, result, lines_out)
            elif op == "audio_mime":
                result = _audio_extension_for_mime(a)
                emit(op, a, result, lines_out)
            elif op == "looks_binary":
                if len(a) == 0:
                    data = b""
                else:
                    data = bytes.fromhex(a)
                result = "true" if _fs_looks_binary(data) else "false"
                emit(op, a, result, lines_out)
            else:
                # mirror C: skip unknown ops
                continue
    return "\n".join(lines_out) + ("\n" if lines_out else "")


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_web_server_pure.py <fixture>\n")
        sys.exit(2)
    out = run(sys.argv[1])
    sys.stdout.write(out)


if __name__ == "__main__":
    main()
