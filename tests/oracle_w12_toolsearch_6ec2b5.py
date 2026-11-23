#!/usr/bin/env python3
"""Differential oracle for tools/tool_search_catalog.py (gate w12_toolsearch_6ec2b5)."""
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
G = "w12_toolsearch_6ec2b5"
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


class _Stemmer:
    def stemWord(self, token):
        return token


_seed("snowballstemmer", stemmer=lambda *a, **k: _Stemmer())

SRC = QUARRY + "/tools/tool_search_catalog.py"
_spec = importlib.util.spec_from_file_location("tool_search_catalog_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["tool_search_catalog_live"] = live
_spec.loader.exec_module(live)

_FUNCS = ("short_desc", "listing_group_label", "fn",
          "required_term_coverage", "entry_search_text")


def live_out(rec):
    func = rec.get("func")
    if func == "short_desc":
        return live._short_desc(rec.get("description") or "", int(rec.get("max_chars", 80)))
    if func == "listing_group_label":
        return live._listing_group_label(rec.get("source_name") or "")
    if func == "fn":
        return live._fn(rec.get("td") or {})
    if func == "required_term_coverage":
        return live._required_term_coverage(int(rec.get("n") or 0))
    if func == "entry_search_text":
        return live._entry_search_text(rec.get("td") or {}, rec.get("source_label") or "")
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
