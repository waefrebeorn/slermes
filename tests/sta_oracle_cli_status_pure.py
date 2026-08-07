"""Oracle for cli.py _status_bar_goal_segment + _fmt_stash_age."""
import json, sys

DEV_ROOT = "/home/wubu/hermes-agent-dev"
if DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

import cli as cli_mod
import time as _time

cases_file = sys.argv[1] if len(sys.argv) > 1 else "cases.in"
with open(cases_file) as f:
    cases = json.load(f)

# _status_bar_goal_segment / _fmt_stash_age are @staticmethod on HermesCLI
_HCLI = cli_mod.HermesCLI

for c in cases:
    op = c.get("op", "")
    if op == "_status_bar_goal_segment":
        print(json.dumps(_HCLI._status_bar_goal_segment(json.loads(c.get("snapshot", "{}")))))
    elif op == "_fmt_stash_age":
        delta = c.get("delta_secs", 60.0)
        stashed_at = _time.monotonic() - delta
        print(json.dumps(_HCLI._fmt_stash_age(stashed_at)))
