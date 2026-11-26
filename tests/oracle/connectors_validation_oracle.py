#!/usr/bin/env python3
"""Differential oracle for the ported connectors validation layer.

`w19_tool_6f4666_manage_connections` is in the tree but was never verified. Its validation half is
pure -- normalize_targets + validate_action -- so it can be compared with upstream directly. The
execution half needs the hosted portal client and is out of scope here (and out of reach in C).

Every case is one that upstream refuses *before* any I/O, so both sides must produce the same error
string. A case that upstream accepts is a bug in the case list, not a pass.

Usage: python tests/oracle/connectors_validation_oracle.py [--keep]
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

QUARRY = Path(os.environ.get("HERMES_QUARRY", r"C:/Users/eman5/hermes-agent"))
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(QUARRY))

from tools.connectors.tool import manage_connections  # noqa: E402


def cases() -> list[dict]:
    return [
        {"name": "connectors-not-a-list",
         "args": {"action": "status", "connectors": 42}},
        {"name": "connectors-list-of-ints",
         "args": {"action": "status", "connectors": [1, 2]}},
        {"name": "unknown-action",
         "args": {"action": "bogus", "connectors": ["github"]}},
        {"name": "no-action",
         "args": {"connectors": ["github"]}},
        {"name": "to-without-rename",
         "args": {"action": "status", "connectors": ["github"], "to": "work"}},
        {"name": "rename-two-targets",
         "args": {"action": "rename", "connectors": ["github", "slack"], "to": "work"}},
        {"name": "rename-no-new-name",
         "args": {"action": "rename", "connectors": ["github"]}},
        {"name": "mcp-action-on-managed",
         "args": {"action": "add", "connectors": ["github"]}},
        {"name": "managed-action-on-mcp",
         "args": {"action": "connect", "connectors": [{"name": "fs", "mcp": True}]}},
        {"name": "empty-name",
         "args": {"action": "status", "connectors": [""]}},
        {"name": "object-without-name",
         "args": {"action": "status", "connectors": [{"mcp": True}]}},
    ]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()

    case_list = cases()
    tin = REPO / ".testbin" / "conn_cases.json"
    tout = REPO / ".testbin" / "conn_out.json"
    tin.parent.mkdir(exist_ok=True)
    tin.write_text(json.dumps(case_list), encoding="utf-8")

    exe = REPO / ".testbin" / "conn_diff.exe"
    if not exe.exists():
        build = subprocess.run(
            ["gcc", "-O1", "-w", "-I", "include", "-I", "lib", "-I", "lib/libjson",
             "src/tools/connectors/tool.c", "tests/unit/conn_diff.c", "lib/libjson/json.o",
             "-o", str(exe)],
            cwd=str(REPO), capture_output=True, text=True)
        if build.returncode != 0:
            print("cannot build the harness:")
            print(build.stderr[-2500:])
            return 2

    proc = subprocess.run([str(exe), str(tin), str(tout)], capture_output=True, text=True)
    if proc.returncode != 0:
        print("harness failed:", proc.returncode, proc.stderr[-800:])
        return 2
    got = {c["name"]: c["result"] for c in json.loads(tout.read_text(encoding="utf-8"))}

    agree = failed = 0
    accepted: list[str] = []
    gateway_blocked: list[str] = []
    for case in case_list:
        name = case["name"]
        want = json.loads(manage_connections(case["args"]))
        have = got.get(name)
        if "error" not in want:
            # Upstream accepted it, so it would go on to the hosted client and there is nothing to
            # compare in C. Counted apart, never as agreement.
            accepted.append(name)
            continue
        # A gateway we cannot reach fails before upstream ever validates its arguments, so its answer
        # says nothing about validation. Comparing against it reported three "problems" that were the
        # missing portal token, not a disagreement in C.
        if "portal access token" in str(want.get("error", "")):
            gateway_blocked.append(name)
            continue
        if have == want:
            agree += 1
        else:
            failed += 1
            print(f"FAIL {name}")
            print(f"   want: {want}")
            print(f"   got:  {have}")

    print(f"connectors-validation oracle: {agree} agree, {len(accepted)} accepted by upstream "
          f"(execution path, not compared), {len(gateway_blocked)} blocked by the gateway, "
          f"{failed} problem(s)")
    if accepted:
        print(f"  not compared: {', '.join(accepted)}")
    if gateway_blocked:
        print(f"  gateway unavailable: {', '.join(gateway_blocked)}")
    if not args.keep:
        for p in (tin, tout):
            p.unlink(missing_ok=True)
        exe.unlink(missing_ok=True)
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
