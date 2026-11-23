#!/usr/bin/env python3
"""Differential oracle for tools/memory_tool_store.py (gate w10_memorytool_0052c2)."""
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
G = "w10_memorytool_0052c2"
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


_seed("tools")
_seed("utils", atomic_write_text=lambda *a, **k: None)
_seed("tools.threat_patterns", first_threat_message=lambda *a, **k: None)

SRC = QUARRY + "/tools/memory_tool_store.py"
_spec = importlib.util.spec_from_file_location("memory_tool_store_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["memory_tool_store_live"] = live
_spec.loader.exec_module(live)


def live_out(rec):
    func = rec.get("func")
    if func == "normalize_for_match":
        return live._normalize_for_match(rec.get("text") or "")
    if func == "span_occurrences":
        return live._span_occurrences(rec.get("text") or "", rec.get("span") or "")
    if func == "repeated_span_message":
        return live._repeated_span_message(rec.get("old_text") or "", int(rec.get("count") or 0))
    if func == "over_limit":
        return live._over_limit(int(rec.get("new_total") or 0), int(rec.get("limit") or 0))
    if func == "stale_entry_message":
        return live._stale_entry_message(rec.get("entry") or "")
    if func == "find_unique_match":
        idx, amb = live._find_unique_match(rec.get("entries") or [], rec.get("old_text") or "")
        return [idx, amb]
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
        if rec.get("func") not in ("normalize_for_match", "span_occurrences", "repeated_span_message",
                                   "over_limit", "stale_entry_message", "find_unique_match"):
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
