#!/usr/bin/env python3
"""Oracle for cron/jobs.py:rewrite_skill_refs (LIVE bodies, stubbed I/O)."""
import ast
import copy
import json
import sys
from contextlib import nullcontext

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/cron/jobs.py"
NS = {"str": str, "list": list, "dict": dict, "set": set}
tree = ast.parse(open(SRC, encoding="utf-8").read())
body = []
for n in tree.body:
    if isinstance(n, ast.FunctionDef) and n.name in {"rewrite_skill_refs", "_normalize_skill_list"}:
        n.decorator_list = []
        body.append(n)
exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), NS)
live = NS["rewrite_skill_refs"]
if not live:
    print("FATAL oracle_cron_jobs_rewrite: missing LIVE body")
    sys.exit(2)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line or '"args"' not in line:
        continue
    r = json.loads(line)
    jobs_in = copy.deepcopy(r["jobs"])
    a = r["args"] or {}
    loaded = []
    saved = []

    def load_jobs():
        d = copy.deepcopy(jobs_in)
        loaded.append(d)
        return d

    def save_jobs(jobs):
        saved.append(copy.deepcopy(jobs))

    NS.update({
        "_jobs_lock": lambda: nullcontext(),
        "load_jobs": load_jobs,
        "save_jobs": save_jobs,
        "logger": type("L", (), {"info": staticmethod(lambda *a, **k: None)})(),
    })
    try:
        want_result = live(a.get("consolidated"), a.get("pruned"))
    except BaseException as exc:
        want_result = {"__raise__": type(exc).__name__, "message": str(exc)}
    want = {"result": want_result, "jobs": saved[0] if saved else loaded[0] if loaded else jobs_in}
    got = r["out"]
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH args={a} jobs={jobs_in} want={want!r} got={got!r}", file=sys.stderr)
print(f"ORACLE cron_jobs_rewrite: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
