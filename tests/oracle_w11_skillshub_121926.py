#!/usr/bin/env python3
"""Differential oracle for tools/skills_hub_sources.py (gate w11_skillshub_121926)."""
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
G = "w11_skillshub_121926"
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
_seed("tools.skills_hub_models",
      GuardedFetchMixin=_GuardedFetchMixin, SkillSource=_SkillSource,
      SkillBundle=dict, SkillMeta=dict)

SRC = QUARRY + "/tools/skills_hub_sources.py"
_spec = importlib.util.spec_from_file_location("skills_hub_sources_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["skills_hub_sources_live"] = live
_spec.loader.exec_module(live)

WK = live.WellKnownSkillSource
US = live.UrlSource
LH = live.LobeHubSource
BS = live.BrowseShSource
_FUNCS = ("query_to_index_url", "wrap_identifier", "parse_wellknown_identifier",
          "is_valid_skill_name", "resolve_skill_name", "url_matches",
          "agent_id", "slug_from_identifier")


def live_out(rec):
    func = rec.get("func")
    if func == "query_to_index_url":
        return WK()._query_to_index_url(rec.get("query") or "")
    if func == "wrap_identifier":
        return WK._wrap_identifier(rec.get("base_url") or "", rec.get("skill_name") or "")
    if func == "parse_wellknown_identifier":
        return WK()._parse_identifier(rec.get("identifier") or "")
    if func == "is_valid_skill_name":
        return US._is_valid_skill_name(rec.get("name"))
    if func == "resolve_skill_name":
        return US._resolve_skill_name(rec.get("fm"), rec.get("url") or "")
    if func == "url_matches":
        return US()._matches(rec.get("identifier"))
    if func == "agent_id":
        return LH._agent_id(rec.get("identifier") or "")
    if func == "slug_from_identifier":
        return BS()._slug_from_identifier(rec.get("identifier") or "")
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
