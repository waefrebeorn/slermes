#!/usr/bin/env python3
"""Differential oracle: C tool_search_dispatch_search vs Python tools/tool_search_catalog.

Both sides get the same six tool schemas and the same queries; the ranking must agree, because the
dispatcher is what decides which deferred tool the model is even shown.

    python tests/oracle/tool_search_oracle.py [-v]
"""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
EXE = REPO / ".testbin" / "tool_search_diff.exe"
QUARRY = Path("C:/Users/eman5/hermes-agent")

SCHEMAS = [
    ("vault_get_secret", "Read a stored API key or password from the encrypted vault by name.", ["name", "reveal"]),
    ("vault_list_items", "List the items in the encrypted vault without revealing their secrets.", ["category"]),
    ("browser_navigate", "Open a URL in the browser and return the page title.", ["url"]),
    ("mail_send", "Send an email message to one or more recipients.", ["to", "subject", "body"]),
    ("mail_search", "Search the mail archive for messages matching a query.", ["query"]),
    ("kanban_add_task", "Create a task on the kanban board.", ["title", "column"]),
]

QUERIES = ["vault secrets", "browser navigate", "send email", "kanban task", "secret password", "zzz_nothing"]


def schema_json(name, desc, params):
    return json.dumps({"type": "function", "function": {
        "name": name, "description": desc,
        "parameters": {"type": "object", "properties": {p: {"type": "string"} for p in params}}}})


def build():
    if EXE.exists():
        EXE.unlink()
    cc = "C:/winlibs/mingw64/bin/gcc.exe"
    cmd = [cc, "-O1", "-w", "-include", "hermes_win32.h", "-D_GNU_SOURCE",
           "-I", "include/compat", "-I", "include", "-I", "src", "-I", "lib", "-I", "lib/libjson",
           "src/tools/tool_search.c", "tests/unit/tool_search_diff.c", "lib/libjson/json.o",
           "-L", "C:/winlibs/extra-libs", "-ltre", "-o", str(EXE)]
    r = subprocess.run(cmd, cwd=str(REPO), capture_output=True, text=True)
    if r.returncode != 0:
        print("cannot build the harness:\n" + (r.stderr or "")[:900])
        sys.exit(2)


def main() -> int:
    verbose = "-v" in sys.argv
    build()

    sys.path.insert(0, str(QUARRY))
    try:
        from tools.tool_search_catalog import build_catalog, search_catalog
    except Exception as e:                                     # noqa: BLE001
        print(f"cannot import the Python catalog: {type(e).__name__}: {e}")
        return 2

    defs = [json.loads(schema_json(n, d, p)) for n, d, p in SCHEMAS]
    catalog = build_catalog(defs)

    proc = subprocess.run([str(EXE)], input="\n".join(QUERIES) + "\n",
                          capture_output=True, text=True, cwd=str(REPO))
    if proc.returncode != 0:
        print(f"harness failed rc={proc.returncode}: {proc.stderr[:300]}")
        return 2

    c_hits, current = {}, None
    for line in proc.stdout.splitlines():
        if line.startswith("Q "):
            current = line[2:]
            c_hits[current] = []
        elif line.startswith("H ") and current:
            rest = line[2:]
            if rest.strip() == "none":
                continue
            parts = rest.split("\t")
            if len(parts) >= 2:
                c_hits[current].append(parts[1])
        elif line.startswith("RAW ") and current:
            c_hits[current].append("<raw:" + line[4:60] + ">")

    problems = 0
    for q in QUERIES:
        py = [e.name for e in search_catalog(catalog, q, limit=3)]
        c = c_hits.get(q, [])
        if py != c:
            problems += 1
            print(f"DISAGREE [{q}]")
            print(f"    C : {c}")
            print(f"    py: {py}")
        elif verbose:
            print(f"agree [{q}]: {c}")
    print(f"tool-search oracle: {len(QUERIES) - problems} agree, {problems} problem(s)")
    return 0 if problems == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
