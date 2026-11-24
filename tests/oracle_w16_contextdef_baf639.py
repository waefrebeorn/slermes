#!/usr/bin/env python3
"""Differential oracle for gate w16_contextdef_baf639 (agent/context_default_record.py).

The live module is loaded via spec_from_file_location. hermes_constants.hermes_home_key
is stubbed so the profile home is injected per case; the injected flag/outer state
drives records_context_defaults through a fake resolver that calls the LIVE
mark_context_defaulted(). Compares the C harness `out` to the live result.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if not (Path(QUARRY) / "agent" / "context_default_record.py").is_file():
    QUARRY = str(Path(__file__).resolve().parent.parent.parent / "hermes-agent")
sys.path.insert(0, QUARRY)
for _pkg in ("agent", "hermes_cli"):
    if _pkg not in sys.modules:
        _m = types.ModuleType(_pkg)
        _m.__path__ = [str(Path(QUARRY) / _pkg)]
        sys.modules[_pkg] = _m

_home = {"v": ""}
_hc = types.ModuleType("hermes_constants")
_hc.hermes_home_key = lambda *a, **k: _home["v"]
sys.modules["hermes_constants"] = _hc

_SPEC = importlib.util.spec_from_file_location(
    "w16_contextdef_live", str(Path(QUARRY) / "agent" / "context_default_record.py"))
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, list): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def _load_records(rec):
    mod._RECORDS = {}
    for r in rec.get("records") or []:
        mod._RECORDS[mod._key(r.get("model"), r.get("base_url"))] = int(r.get("window") or 0)


def expected(rec):
    f = rec.get("func")
    _home["v"] = rec.get("profile_home") or ""
    if f == "key":
        return list(mod._key(rec.get("model"), rec.get("base_url")))
    if f == "was_context_defaulted":
        _load_records(rec)
        return bool(mod.was_context_defaulted(int(rec.get("context_length") or 0),
                                              rec.get("model"), rec.get("base_url") or ""))
    if f == "record_resolution":
        _load_records(rec)
        mod._MAX_RECORDS = int(rec.get("max_records") or 512)
        outer = [False] if rec.get("outer_present") else None
        def resolve(model=None, base_url=""):
            if rec.get("flag"):
                mod.mark_context_defaulted()
            return int(rec.get("context_length") or 0)
        wrapper = mod.records_context_defaults(resolve)
        tok = mod._DEFAULTED.set(outer) if outer is not None else None
        try:
            wrapper(model=rec.get("model"), base_url=rec.get("base_url") or "")
        finally:
            if tok is not None:
                mod._DEFAULTED.reset(tok)
        out_recs = [{"model": k[1], "base_url": k[2], "window": v}
                    for k, v in mod._RECORDS.items()]
        return {"records": out_recs,
                "outer_set": bool(outer[0]) if outer is not None else False,
                "last_defaulted": mod.last_defaulted_context_length()}
    raise RuntimeError("unknown func " + str(f))


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        rec = json.loads(line)
        n += 1
        got = rec.get("out")
        if isinstance(got, dict) and isinstance(got.get("records"), list):
            got["records"] = sorted(got["records"],
                                    key=lambda d: (d.get("model", ""), d.get("base_url", ""), d.get("window", 0)))
        want = expected(rec)
        if _canon(got) != _canon(want):
            mism += 1
            if mism <= 10:
                print("MISMATCH %s\n  got : %s\n  want: %s" %
                      (rec.get("func"), json.dumps(got, ensure_ascii=False),
                       json.dumps(want, ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w16_contextdef_baf639: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
