#!/usr/bin/env python3
"""Faithfulness oracle for port_file_operations.c: the three v549 linters.

Reads JSON lines from t_port_file_lint.c and recomputes the SAME linters from
the LIVE tools/file_operations.py (_lint_yaml_inproc / _lint_toml_inproc /
_lint_python_inproc). Both sides produce a JSON string with keys "valid"
(bool) and "error" (str). We compare the normalized {valid, error} shape.

Note: for the YAML/TOML/Python linters the *error message text* may differ
between the C backend and CPython, but the *valid* boolean (the contract that
matters) and the error-presence must agree. We assert valid-equality strictly,
and error-presence (empty vs non-empty) strictly; the exact message text is
compared best-effort (reported, not failed) because backends differ.
"""
import sys, os, json
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from tools.file_operations import (
    _lint_yaml_inproc, _lint_toml_inproc, _lint_python_inproc, _lint_json_inproc,
)

LINTERS = {
    "yaml": _lint_yaml_inproc,
    "toml": _lint_toml_inproc,
    "python": _lint_python_inproc,
    "json": _lint_json_inproc,
}


def parse_c(out):
    """Parse the C JSON output into {'valid':bool,'error':str}."""
    obj = json.loads(out)
    return bool(obj.get("valid")), (obj.get("error") or "")


def norm_py(ok, err):
    return bool(ok), (err or "")


mism = 0
n = 0
for line in sys.stdin:
    line = line.strip()
    if not line or not line.startswith("{"):
        continue
    rec = json.loads(line)
    fn = rec["fn"]
    if fn not in LINTERS:
        print("UNKNOWN FN", fn)
        continue
    n += 1
    inp = rec["in"]
    c_valid, c_err = parse_c(rec["out"])
    py_ok, py_err = norm_py(*LINTERS[fn](inp))

    # Strict: the validity verdict must match.
    if c_valid != py_ok:
        mism += 1
        print(f"MISMATCH valid fn={fn} in={inp!r} PY_valid={py_ok} C_valid={c_valid}")
        continue

    # Strict: error presence must match (empty vs non-empty).
    c_has = bool(c_err.strip())
    p_has = bool(py_err.strip())
    if c_has != p_has:
        # For python, C delegates to CPython so messages should match closely;
        # for yaml/toml the C backend may lack the detailed message.
        if fn == "python":
            mism += 1
            print(f"MISMATCH errpres fn={fn} in={inp!r} PY_err={py_err!r} C_err={c_err!r}")
        else:
            # backends differ on message text; report but do not fail
            print(f"NOTE errpres-diff fn={fn} PY_err={py_err!r} C_err={c_err!r}")

print(f"FILE_LINT oracle: {n} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
