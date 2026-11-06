#!/usr/bin/env python3
"""Oracle for UnauthorizedOwnerNotifier.first_time (run_inbound_unauthorized.py).

The harness replays a call sequence against the C LRU set; this oracle drives
the LIVE method's OrderedDict the same way and compares per-call outcomes and the
final set order.
"""
import ast
import json
import sys
from collections import OrderedDict

sys.stdin.reconfigure(encoding="utf-8")
SRC = "C:/Users/eman5/hermes-agent/gateway/run_inbound_unauthorized.py"

# extract the class and build a real instance with a small max_seen
ns = {}
tree = ast.parse(open(SRC, encoding="utf-8").read())
# seed the module global the class body references in its def-default
for n in tree.body:
    if isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name) and \
       n.targets[0].id == "_MAX_SEEN_SENDERS":
        exec(compile(ast.Module(body=[n]), "<c>", "exec"), ns)
        break
for n in tree.body:
    if isinstance(n, ast.ClassDef) and n.name == "UnauthorizedOwnerNotifier":
        exec(compile(ast.Module(body=[n]), "<x>", "exec"), ns)
        C = ns[n.name]
        break

# __init__ takes runner; bypass it and set fields directly.
obj = object.__new__(C)
obj._seen = OrderedDict()
obj._max_seen = 3
outcomes = []
seq = [("tg","a"),("tg","b"),("tg","c"),("tg","a"),
       ("tg","d"),("tg","b"),("tg","e")]
for p, u in seq:
    outcomes.append(1 if obj.first_time(p, u) else 0)

passes = 0
fails = []
total = 0
seen_set = None
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    c = json.loads(line)
    if c["tag"] == "ft":
        total += 1
        if c["out"] == (outcomes[c["case"]] == 1):
            passes += 1
        else:
            fails.append(("ft", c["case"], outcomes[c["case"]], c["out"]))
    elif c["tag"] == "set":
        seen_set = c["keys"]
# compare final set to the live OrderedDict keys
live = list(obj._seen.keys())
want = ["%s/%s" % (k[0], k[1]) for k in live]
if seen_set is not None:
    total += 1
    if seen_set == want:
        passes += 1
    else:
        fails.append(("set", want, seen_set))

print("pass/total: %d/%d" % (passes, total))
for f in fails[:8]:
    print("FAIL", f[0], "in=", f[1], "want=", f[2], "c=", f[3])
if fails:
    print("ORACLE first_time: %d cases, %d mismatches" % (total, len(fails)))
    sys.exit(1)
print("ORACLE first_time: %d cases, 0 mismatches" % total)
