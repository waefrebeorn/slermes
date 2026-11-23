#!/usr/bin/env python3
"""Differential oracle for tools/skills_hub_skillssh.py (gate w12_skillshub_a4dffc)."""
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
G = "w12_skillshub_a4dffc"
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


_seed("tools")
_seed("tools.skills_hub_github", GitHubAuth=_GitHub, GitHubSource=_GitHub,
      _split_repo_id=lambda *a, **k: None)
_seed("tools.skills_hub_models", SkillBundle=dict, SkillMeta=dict,
      SkillSource=_SkillSource, _cache_metas=lambda *a, **k: None,
      _cached_metas=lambda *a, **k: None, _get_json=lambda *a, **k: None,
      _get_text=lambda *a, **k: None, _memo_json=lambda *a, **k: None,
      hub=lambda: None)

SRC = QUARRY + "/tools/skills_hub_skillssh.py"
_spec = importlib.util.spec_from_file_location("skills_hub_skillssh_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["skills_hub_skillssh_live"] = live
_spec.loader.exec_module(live)

_FUNCS = ("strip_html", "extract_repo_slug", "normalize_identifier",
          "wrap_identifier", "detail_to_metadata", "token_variants")


def live_out(rec):
    func = rec.get("func")
    SS = live.SkillsShSource
    if func == "strip_html":
        return live._strip_html(rec.get("value") or "")
    if func == "extract_repo_slug":
        return SS._extract_repo_slug(rec.get("repo_value") or "")
    if func == "normalize_identifier":
        return SS._normalize_identifier(rec.get("identifier") or "")
    if func == "wrap_identifier":
        return SS._wrap_identifier(rec.get("identifier") or "")
    if func == "detail_to_metadata":
        inst = object.__new__(SS)
        return inst._detail_to_metadata(rec.get("canonical") or "", rec.get("detail"))
    if func == "token_variants":
        return sorted(SS._token_variants(rec.get("value")))
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
