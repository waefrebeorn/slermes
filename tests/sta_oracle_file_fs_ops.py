#!/usr/bin/env python3
"""Oracle for v553 file_fs_ops extraction: C == LIVE Python.

Reads JSON lines from tests/t_port_file_fs_ops.c and asserts C output equals
what LIVE tools/file_operations.py (+ agent/file_safety) would produce.

  gcc -O2 -g -I include -I src/tools -I src/agent -I lib/libjson \
      tests/t_port_file_fs_ops.c src/tools/file_fs_ops.o \
      src/tools/file_text_ops.o src/agent/file_safety.o \
      lib/libjson/json.o -o /tmp/t_fs
  /tmp/t_fs | python3 tests/sta_oracle_file_fs_ops.py
"""
import json, sys, os
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, REPO)
import tools.file_operations as fo
import tools.binary_extensions as be
import agent.file_safety as fs

IMAGE = set(fo.IMAGE_EXTENSIONS)
BINARY = set(be.BINARY_EXTENSIONS)

def py_is_image(path):
    ext = os.path.splitext(path)[1].lower()
    return ext in IMAGE

def py_is_likely_binary(path):
    ext = os.path.splitext(path)[1].lower()
    if ext in BINARY:
        return True
    if not os.path.exists(path):
        return False
    with open(path, "rb") as f:
        sample = f.read(1000)
    if not sample:
        return False
    non_printable = sum(1 for c in sample if c < 32 and c not in b"\n\r\t")
    return (non_printable / len(sample)) > 0.30

mismatches = 0
total = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    rec = json.loads(line)
    fn = rec["fn"]
    total += 1

    if fn == "read_file_raw":
        p = rec["in"]
        with open(p, "rb") as f:
            data = f.read()
        exp = data.decode("utf-8", "replace")
        if rec["out"] != exp:
            mismatches += 1
            print(f"MISMATCH {fn} {p!r} C={rec['out']!r} PY={exp!r}")

    elif fn == "is_image":
        p = rec["in"]
        exp = py_is_image(p)
        if (rec["out"] is True) != exp:
            mismatches += 1
            print(f"MISMATCH {fn} {p!r} C={rec['out']} PY={exp}")

    elif fn == "is_likely_binary":
        p = rec["in"]
        exp = py_is_likely_binary(p)
        if (rec["out"] is True) != exp:
            mismatches += 1
            print(f"MISMATCH {fn} {p!r} C={rec['out']} PY={exp}")

    elif fn == "patch_replace":
        content, old, new = rec["in"].split("|")
        exp = content.replace(old, new, 1)
        if rec["out"] != exp:
            mismatches += 1
            print(f"MISMATCH {fn} in={rec['in']!r} C={rec['out']!r} PY={exp!r}")

    elif fn == "detect_file_line_ending":
        p = rec["in"]
        with open(p, "rb") as f:
            content = f.read().decode("utf-8", "replace")
        r = fo._detect_line_ending(content)   # "\r\n" / "\n" / None
        exp = {"\r\n": "crlf", "\n": "lf"}.get(r, "unknown")
        if rec["out"] != exp:
            mismatches += 1
            print(f"MISMATCH {fn} {p!r} C={rec['out']!r} PY={exp} (raw={r!r})")

    elif fn == "file_has_bom":
        p = rec["in"]
        with open(p, "rb") as f:
            head = f.read(3)
        exp = (head == b"\xef\xbb\xbf")
        if (rec["out"] is True) != exp:
            mismatches += 1
            print(f"MISMATCH {fn} {p!r} C={rec['out']} PY={exp}")

    elif fn == "delete_path_denied":
        p = rec["in"]
        denied = fs.is_write_denied(p)   # True -> C returns false
        exp = not denied
        if (rec["out"] is True) != exp:
            mismatches += 1
            print(f"MISMATCH {fn} {p!r} C={rec['out']} PY_denied={denied}")

    elif fn == "delete_path_ok":
        # a non-denied existing file should delete successfully -> true
        if rec["out"] is not True:
            mismatches += 1
            print(f"MISMATCH {fn} {rec['in']!r} C={rec['out']} (expected true)")

print(f"{total} cases, {mismatches} mismatches")
sys.exit(1 if mismatches else 0)
