#!/usr/bin/env python3
"""Differential oracle for gate w10_skillshub_6c5e0d (tools/skills_hub_models.py)."""
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

SRC = Q + "/tools/skills_hub_models.py"
spec = importlib.util.spec_from_file_location("w10shm_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def _meta(**kw):
    d = dict(name="n", description="d", source="github", identifier="o/r/s", trust_level="community")
    d.update(kw)
    return live.SkillMeta(**d)


def compute(r):
    f = r.get("func")
    if f == "matches":
        return live._matches_query(r.get("query", ""), *(r.get("fields") or []))
    if f == "dedupe":
        items = r.get("results") or []
        keys = set().union(*[set(i) for i in items]) if items else set()
        metas = [_meta(**item) for item in items]
        kept = live._dedupe_by_trust(metas)
        return [{k: v for k, v in vars(m).items() if k in keys} for m in kept]
    if f == "normpath":
        try:
            return live._normalize_bundle_path(r.get("path", ""), field_name=r.get("field", "bundle file path"),
                                               allow_nested=bool(r.get("nested", True)))
        except ValueError:
            return {"__raise__": "ValueError"}
    if f == "valname":
        try:
            return live._validate_skill_name(r.get("name", ""))
        except ValueError:
            return {"__raise__": "ValueError"}
    if f == "valparent":
        try:
            return live._validate_install_parent_path(r.get("category", ""))
        except ValueError:
            return {"__raise__": "ValueError"}
    if f == "valrel":
        try:
            return live._validate_bundle_rel_path(r.get("rel_path", ""))
        except ValueError:
            return {"__raise__": "ValueError"}
    if f == "hermestags":
        return live._hermes_tags(r.get("fm") or {})
    if f == "concrete":
        return live._query_is_concrete(r.get("query", ""))
    if f == "srcurl":
        b = types.SimpleNamespace(metadata=(r.get("bundle") or {}).get("metadata") or {},
                                  source=(r.get("bundle") or {}).get("source", ""),
                                  identifier=(r.get("bundle") or {}).get("identifier", ""))
        return live.source_url_for_bundle(b)
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
print("ORACLE w10_skillshub_6c5e0d: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if (mism or not cases) else 0)
