#!/usr/bin/env python3
"""Differential oracle for gateway/session_listing.py (gate w9_sessionlist_96be7b).

Loads the LIVE module via importlib and compares the C harness `out` for the
pure cursor / page-param functions: check_page_params, _binding_digest,
_encode_cursor and _decode_cursor.
Summary: ORACLE w9_sessionlist_96be7b: N cases, M mismatches.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w9_sessionlist_96be7b"
sys.path.insert(0, QUARRY)


def _seed(name, **attrs):
    mod = types.ModuleType(name)
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


try:
    import gateway  # noqa: F401
except Exception:
    _seed("gateway")

SRC = QUARRY + "/gateway/session_listing.py"
_spec = importlib.util.spec_from_file_location("session_listing_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["session_listing_live"] = live
_spec.loader.exec_module(live)

AuthorityRefusal = __import__("persistence.core.errors", fromlist=["AuthorityRefusal"]).AuthorityRefusal


def _ok_or_refuse(call):
    try:
        call()
        return {"ok": True}
    except AuthorityRefusal as exc:
        return {"error": exc.reason, "field": exc.field}


def _val_or_refuse(call):
    try:
        return call()
    except AuthorityRefusal as exc:
        return {"error": exc.reason, "field": exc.field}


def live_out(rec):
    func = rec.get("func")
    if func == "check_page_params":
        return _ok_or_refuse(lambda: live.check_page_params(rec.get("params")))
    ws = rec.get("workspace")
    q = rec.get("query")
    inc = bool(rec.get("include_hidden", False))
    binding = (ws, q, inc)
    if func == "binding_digest":
        return live._binding_digest(binding)
    if func == "encode_cursor":
        return live._encode_cursor(int(rec["offset"]), binding)
    if func == "decode_cursor":
        return _val_or_refuse(lambda: live._decode_cursor(rec.get("cursor"), binding))
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
        if rec.get("func") not in ("check_page_params", "binding_digest", "encode_cursor", "decode_cursor"):
            continue
        cases += 1
        py = live_out(rec)
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
