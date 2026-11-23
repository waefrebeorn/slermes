#!/usr/bin/env python3
"""Differential oracle for tools/skills_hub_search.py (gate skillsearch)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"; sys.path.insert(0, Q)
PROVIDERS = {"openai","anthropic","huggingface","nvidia","voltagent","gstack","minimax",
             "weights & biases","qdrant","pinecone","notion","airtable","shopify"}

def stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn); pm.__path__ = [os.path.join(Q, *parts[:i])]; sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items(): setattr(m, k, v)
    sys.modules[name] = m
    return m

def _pf(sf):
    v = sf.strip().lower()
    return v if v in PROVIDERS else ""

stub("httpx", DecodingError=type("DecodingError",(Exception,),{}), HTTPError=type("HTTPError",(Exception,),{}))
for mod, names in [
    ("tools.skills_hub_clawhub", ["ClawHubSource"]),
    ("tools.skills_hub_models", ["SkillMeta","SkillSource","TRUST_RANK","_dedupe_by_trust","hub"]),
    ("tools.skills_hub_official", ["HermesIndexSource","OptionalSkillSource"]),
    ("tools.skills_hub_skillssh", ["SkillsShSource"]),
    ("tools.skills_hub_sources", ["BrowseShSource","LobeHubSource","UrlSource","WellKnownSkillSource"]),
]:
    stub(mod, **{n: type(n,(),{}) for n in names})
stub("tools.skills_hub_github", GitHubAuth=type("GitHubAuth",(),{}), GitHubSource=type("GitHubSource",(),{}),
     _filter_results_by_provider=lambda results, provider: results, _provider_filter_of=_pf)

parts = "tools/skills_hub_search".split("/")
for i in range(1, len(parts)):
    pn = ".".join(parts[:i])
    if pn not in sys.modules:
        pm = types.ModuleType(pn); pm.__path__ = [os.path.join(Q, *parts[:i])]; sys.modules[pn] = pm
name = ".".join(parts)
sp = importlib.util.spec_from_file_location(name, os.path.join(Q, "tools/skills_hub_search.py"))
live = importlib.util.module_from_spec(sp); sys.modules[name] = live; sp.loader.exec_module(live)

class S:
    def __init__(self, sid, avail=False): self.sid = sid; self.is_available = avail
    def source_id(self): return self.sid

def compute(r):
    f = r.get("func")
    if f == "provider_filter": return live._provider_filter_of(r.get("source_filter", ""))
    if f == "select_active":
        sources = [S(d["id"], bool(d.get("available", False))) for d in r.get("sources", [])]
        active = live._select_active_sources(sources, r.get("source_filter", "all"))
        return [s.source_id() for s in active]
    if f == "index_miss_fallback":
        all_s = [S(x) for x in r.get("all_ids", [])]
        active_ids = set(r.get("active_ids", []))
        # Python uses `src not in active` (object identity): build both lists from
        # the same source objects, mirroring the real call where `active` is a
        # sublist of `sources`.
        active_s = [s for s in all_s if s.sid in active_ids]
        res = live._index_miss_fallback_sources(all_s, active_s, r.get("query", ""),
                                                r.get("source_counts", {}), r.get("provider_filter", ""))
        return [s.source_id() for s in res]
    return None

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    if r.get("func") is None: continue
    cases += 1
    py, got = compute(r), r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH skillsearch %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE skillsearch: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
