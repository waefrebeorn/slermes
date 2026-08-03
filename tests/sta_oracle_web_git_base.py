#!/usr/bin/env python3
"""sta_oracle_web_git_base.py — LIVE Python web_git oracle (imports the real
module; it has no FastAPI deps)."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path("/home/wubu/hermes-agent-dev")))
from hermes_cli import web_git  # noqa: E402


def main():
    fx = json.loads(Path(sys.argv[1]).read_text())
    op, repo = fx["op"], fx["repo"]
    if op == "base_branches":
        print(json.dumps(web_git.base_branch_list(repo), separators=(",", ":")))
    elif op == "review_list_sorted":
        print(json.dumps(web_git.review_list(repo, "uncommitted", None),
                         separators=(",", ":")))
    else:
        raise SystemExit(f"unknown op {op}")


if __name__ == "__main__":
    main()
