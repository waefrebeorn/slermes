#!/usr/bin/env python3
"""Differential oracle for gate w10_skillshub_742005 (tools/skills_hub_github.py)."""
import importlib.util
import json
import sys
import types

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/tools/skills_hub_github.py"
spec = importlib.util.spec_from_file_location("w10shg_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "provider":
        return live.github_provider_for(r.get("repo", ""))
    if f == "provfilter":
        return live._provider_filter_of(r.get("source", ""))
    if f == "cachekey":
        return live._tap_cache_key(r.get("repo", ""), r.get("path", ""), r.get("bucket"))
    if f == "splitid":
        v = live._split_repo_id(r.get("identifier", ""))
        return list(v) if v is not None else None
    if f == "skillpath":
        return live._skill_file_path(r.get("skill_path", ""), r.get("filename", "SKILL.md"))
    if f == "skipbundle":
        return live._skip_bundle_file(r.get("rel_path", ""))
    if f == "ratelimit":
        resp = types.SimpleNamespace(status_code=r.get("status_code", 0),
                                     headers=r.get("headers") or {})
        return live._is_rate_limit_response(resp)
    if f == "treemembers":
        return [list(t) for t in live._tree_members(r.get("entries") or [], r.get("prefix", ""))]
    if f == "groupings":
        return live.GitHubSource._parse_skillsh_groupings(r.get("content", ""))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except BaseException as e:
        py = {"__raise__": type(e).__name__}
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w10_skillshub_742005: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if (mism or not cases) else 0)
