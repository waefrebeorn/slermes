#!/usr/bin/env python3
"""Reference oracle for file_lint. Reads C output records on stdin and emits
same-shape records computed from the LIVE linters in tools.file_operations_lint.
"""
import json
import os
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
from tools.file_operations_lint import (
    _lint_yaml_inproc, _lint_toml_inproc, _lint_python_inproc, _lint_json_inproc,
)

LINTERS = {
    "yaml": _lint_yaml_inproc,
    "toml": _lint_toml_inproc,
    "python": _lint_python_inproc,
    "json": _lint_json_inproc,
}

for line in sys.stdin:
    line = line.strip()
    if not line or not line.startswith("{"):
        continue
    rec = json.loads(line)
    fn = rec["fn"]
    ok, err = LINTERS[fn](rec["in"])
    out = {"valid": bool(ok), "err": 1 if (err or "").strip() else 0}
    print(json.dumps({"fn": fn, "in": rec["in"], "out": out},
                     separators=(",", ":"), ensure_ascii=False))
