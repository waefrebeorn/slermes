#!/usr/bin/env python3
"""
sta_oracle_github_provider.py — Python oracle for github_provider_for
(tools/skills_hub.py:github_provider_for), ported in src/skills_hub.c.

Imports the REAL tools.skills_hub module and calls the genuine function.
Output contract matches tests/t_port_github_provider.c: one JSON object per
line, sorted keys, ensure_ascii=False, compact separators.
"""

import json
import os
import sys

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from tools.skills_hub import github_provider_for  # noqa: E402


def emit(obj):
    sys.stdout.write(json.dumps(obj, separators=(",", ":"), ensure_ascii=False) + "\n")


def main():
    for raw in sys.stdin:
        repo = raw.rstrip("\n")
        if not repo.strip() or repo.startswith("#"):
            continue
        out = github_provider_for(repo if repo else None)
        emit({"in": repo, "out": out})


if __name__ == "__main__":
    sys.exit(main())
