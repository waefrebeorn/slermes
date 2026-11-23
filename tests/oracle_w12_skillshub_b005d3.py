#!/usr/bin/env python3
"""Differential oracle for tools/skills_hub_official.py (gate w12_skillshub_b005d3)."""
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
G = "w12_skillshub_b005d3"
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


class _SkillSource:
    SOURCE_ID = ""


class _GitHub:
    pass


_seed("agent")
_seed("agent.skill_utils", is_excluded_skill_path=lambda *a, **k: False)
_seed("tools")
_seed("tools.skills_hub_github", GitHubAuth=_GitHub, GitHubSource=_GitHub,
      _skip_bundle_file=lambda *a, **k: False, _tree_members=lambda *a, **k: [])
_seed("tools.skills_hub_models", SkillBundle=dict, SkillMeta=dict,
      SkillSource=_SkillSource, _hermes_tags=lambda *a, **k: [],
      _matches_query=lambda *a, **k: False, _memo_json=lambda *a, **k: None,
      _parse_frontmatter=lambda *a, **k: {}, hub=lambda: None)

SRC = QUARRY + "/tools/skills_hub_official.py"
_spec = importlib.util.spec_from_file_location("skills_hub_official_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["skills_hub_official_live"] = live
_spec.loader.exec_module(live)

_FUNCS = ("strip_prefix", "clean_rel_parts", "entry_provider",
          "optional_rel", "upstream_pointer", "to_meta")


def live_out(rec):
    func = rec.get("func")
    if func == "strip_prefix":
        return live._strip_prefix(rec.get("value") or "", rec.get("prefixes") or [])
    if func == "clean_rel_parts":
        return live._clean_rel_parts(rec.get("path") or "")
    if func == "entry_provider":
        return live._entry_provider(rec.get("entry") or {})
    if func == "optional_rel":
        return live.OptionalSkillSource._rel(rec.get("identifier") or "")
    if func == "upstream_pointer":
        return live.OptionalSkillSource._upstream_pointer_from_frontmatter(rec.get("fm") or {})
    if func == "to_meta":
        return live.HermesIndexSource._to_meta(rec.get("entry") or {})
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
