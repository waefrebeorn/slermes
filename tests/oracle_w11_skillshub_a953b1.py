#!/usr/bin/env python3
"""Differential oracle for tools/skills_hub_clawhub.py (gate w11_skillshub_a953b1)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w11_skillshub_a953b1"
sys.path.insert(0, QUARRY)


class _Stub(types.ModuleType):
    def __getattr__(self, name):
        if name.startswith("__"):
            raise AttributeError(name)
        def _noop(*a, **k):
            return None
        return _noop


def _seed(name, **attrs):
    mod = _Stub(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], mod)
    return mod


class _GuardedFetchMixin:
    pass


class _SkillSource:
    SOURCE_ID = ""
    def __init__(self):
        pass


_seed("tools")
_seed("utils")
_seed("agent")
_seed("httpx")
_seed("agent.retry_utils", parse_retry_after_seconds=lambda h: None)
_seed("tools.skills_hub", _guarded_http_stream=lambda *a, **k: None)
_seed("tools.skills_hub_models",
      GuardedFetchMixin=_GuardedFetchMixin, SkillSource=_SkillSource,
      SkillBundle=dict, SkillMeta=dict)

SRC = QUARRY + "/tools/skills_hub_clawhub.py"
_spec = importlib.util.spec_from_file_location("skills_hub_clawhub_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["skills_hub_clawhub_live"] = live
_spec.loader.exec_module(live)

CS = live.ClawHubSource
_FUNCS = ("query_terms", "search_score", "normalize_tags", "owner_from_payload",
          "owner_matches", "parse_identifier", "catalog_identifier",
          "first_str", "coerce_skill_payload")


def live_out(rec):
    func = rec.get("func")
    if func == "query_terms":
        return live._query_terms(rec.get("query") or "")
    if func == "search_score":
        d = rec.get("meta") or {}
        meta = types.SimpleNamespace(identifier=d.get("identifier"),
                                     name=d.get("name"),
                                     description=d.get("description"))
        return live._search_score(rec.get("query") or "", meta)
    if func == "normalize_tags":
        return CS._normalize_tags(rec.get("tags"))
    if func == "owner_from_payload":
        return CS._owner_from_payload(rec.get("data"))
    if func == "owner_matches":
        return CS._owner_matches(rec.get("expected_owner"), rec.get("data"))
    if func == "parse_identifier":
        r = CS._parse_identifier(rec.get("identifier") or "")
        return {"slug": r[0], "owner": r[1]} if r else None
    if func == "catalog_identifier":
        return CS.catalog_identifier(rec.get("identifier"))
    if func == "first_str":
        return live._first_str(*(rec.get("values") or []))
    if func == "coerce_skill_payload":
        return CS._coerce_skill_payload(rec.get("data"))
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in _FUNCS:
            continue
        cases += 1
        try:
            py = live_out(rec)
        except Exception as exc:
            py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} func={rec.get('func')} "
                  f"C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
