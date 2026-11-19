#!/usr/bin/env python3
"""build_watercup_index — derive the watercup symbol index from THIS tree.

The C modules self-describe: every ported function carries a
/* PoP: <c_fn> @ <py_module>:<py_fn> */ annotation next to its definition.
This script reads those annotations and the actual C signatures from the
local source tree and emits tests/watercup/pop_index.json with RELATIVE
posix paths — no machine-specific absolute paths, regenerable on any OS
via `make watercup-index`. The committed Linux-era index is gone; the
index is a build artifact, not source.
"""
import json
import os
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tests" / "watercup"))
os.chdir(REPO)  # fix_all_gates derives REPO from CWD at import

from fix_all_gates import read_c_signature, SIG_RE  # noqa: E402

POP_RE = re.compile(
    r"/\*\s*PoP:\s*([A-Za-z_]\w*)\s*@\s*([A-Za-z0-9_./\\-]+\.py):([A-Za-z_][\w.]*)")

# Parse each file ONCE: symbol -> (file, signature). Tags then look up in O(1).
DEF_RE = re.compile(
    r"(?:^|\n)[A-Za-z_][A-Za-z_0-9 \t\*]*?\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*\{")
sym_home = {}
sym_sig = {}
file_tags = {}
nfiles = 0
for root in ("src", "lib"):
    for p in (REPO / root).rglob("*.c"):
        try:
            txt = p.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        rel = p.relative_to(REPO).as_posix()
        nfiles += 1
        for m in SIG_RE.finditer(txt):
            fn = m.group(2)
            if fn not in sym_sig:
                params = m.group(3).strip()
                sym_sig[fn] = (m.group(1).strip(), params,
                               [x.strip() for x in params.split(",") if x.strip()])
                sym_home[fn] = rel
        tags = POP_RE.findall(txt)
        if tags:
            file_tags[rel] = tags

index = {}
no_sig = 0
for rel, tags in sorted(file_tags.items()):
    for c_fn, py_mod, py_fn in tags:
        py_mod = py_mod.replace("\\", "/")
        key = f"{py_mod}:{py_fn}"
        sig = sym_sig.get(c_fn)
        if sig is None:
            no_sig += 1
            continue
        ret_type, params, param_list = sig
        if key not in index:
            index[key] = {"c_fn": c_fn, "c_file": sym_home[c_fn],
                          "ret_type": ret_type, "params": params,
                          "param_list": param_list}

out = REPO / "tests" / "watercup" / "pop_index.json"
out.write_text(json.dumps(index, indent=1) + "\n", encoding="utf-8", newline="\n")
print(f"watercup index: {len(index)} entries from {nfiles} C files "
      f"({no_sig} tags without a resolvable signature skipped)")
