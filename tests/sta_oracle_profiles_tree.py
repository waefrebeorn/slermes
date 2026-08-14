#!/usr/bin/env python3
"""Oracle for the pure project-tree merge helpers of
hermes_cli/web_routers/profiles.py ported to C.

Reads the JSON cases fixture (argv[1]), recomputes each case from LIVE
hermes_cli/web_routers/profiles.py, emits one JSON object per case.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.getcwd())
sys.path.insert(0, "/home/wubu/.hermes/hermes-agent")

from hermes_cli.web_routers import profiles as m  # noqa: E402


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op")
        if op == "merge_by_id":
            into = json.loads(c["into"])
            entries = json.loads(c["entries"])
            m._merge_by_id(into, entries, c.get("child_key", "groups"))
            print(json.dumps({"op": "merge_by_id", "out": into}))
        elif op == "merge_profile_tree":
            merged = json.loads(c["merged"])
            projects = json.loads(c["projects"])
            m._merge_profile_tree(merged, projects, c.get("profile", "default"),
                                  int(c.get("preview_limit", 3)))
            print(json.dumps({"op": "merge_profile_tree", "out": merged}))
        elif op == "pr_url":
            r = m._pr_url_from_tool_output(c.get("arg", ""))
            print(json.dumps({"op": "pr_url", "out": r}))
        else:
            print(json.dumps({"op": "unknown"}))


if __name__ == "__main__":
    main()
