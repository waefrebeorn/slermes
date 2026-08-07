"""Oracle for hermes_cli/approvals_suggest.py build_proposals + add_example."""
import json, sys

DEV_ROOT = "/home/wubu/hermes-agent-dev"
if DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

from hermes_cli.approvals_suggest import (
    build_proposals,
    Proposal,
    normalize_command,
)

cases_file = sys.argv[1] if len(sys.argv) > 1 else "cases.in"

with open(cases_file) as f:
    cases = json.load(f)

for c in cases:
    op = c.get("op", "")
    if op == "build_proposals":
        records = c.get("records", [])
        existing = set(c.get("existing", []))
        min_count = c.get("min_count", 2)
        limit = c.get("limit", 20)
        props = build_proposals(records, existing=existing, min_count=min_count, limit=limit)
        out = []
        for p in props:
            out.append({
                "pattern": p.pattern,
                "kind": p.kind,
                "count": p.count,
                "classes": sorted(p.classes),
                "examples": sorted(p.examples),
            })
        print(json.dumps(out, sort_keys=True))
    elif op == "add_example":
        p = Proposal(pattern="git push *", kind="glob")
        for ex in c.get("examples", []):
            p.add_example(ex)
        result = {
            "pattern": p.pattern,
            "kind": p.kind,
            "count": p.count,
            "classes": sorted(list(p.classes)),
            "examples": sorted(p.examples),
        }
        print(json.dumps(result, sort_keys=True))
    elif op == "normalize_command":
        print(json.dumps(normalize_command(c.get("command", ""))))
