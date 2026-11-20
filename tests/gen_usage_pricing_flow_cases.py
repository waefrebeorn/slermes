#!/usr/bin/env python3
"""Generate tests/fixtures/usage_pricing_flow.jsonl for the up2 port.

Each line: {"func": <live_name>, "in": {...inputs..., "io": {seam scripts},
"state": {rejected seed}}}. Script semantics (shared with the C harness
dispatcher and the oracle Script):
  - absent key            -> fail-closed RuntimeError "no script for <name>"
  - non-list value        -> constant result reused every call
  - list value            -> per-call queue (exhausted -> fail-closed)
  - {"__raise__":K,"message":M} -> raised exception of kind K
  - {"__script__":true,"ret":X} -> list-valued result X (queue-ambiguity wrap)
  - null                  -> None result
>=5 cases per function; covers branches, seam order, raises, BaseException
passthrough and fail-closed paths. ASCII inputs; Decimal travels as
{"__dec__":str}; datetime as integer epoch-us; carriers full-field.

--check mode re-derives the constant tables from the LIVE module (via the
oracle's AST-seeded namespace) and diffs them against the JSON literals embedded
in src/agent/usage_pricing_flow.c (between UP2_TABLE_BEGIN/END markers).
"""
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "usage_pricing_flow.jsonl"
PORT_C = HERE.parent / "src" / "agent" / "usage_pricing_flow.c"
CASES = []


def add(func, inp, io=None, state=None):
    inp = dict(inp)
    inp["io"] = io or {}
    if state is not None:
        inp["state"] = state
    CASES.append({"func": func, "in": inp})


def R(kind, msg=""):
    return {"__raise__": kind, "message": msg}


def L(ret):
    return {"__script__": True, "ret": ret}


def dec(s):
    return {"__dec__": s}


def E(inp=None, out=None, cr=None, cw=None, cw1h=None, req=None, source="none",
      url=None, ver=None, fetched=None, thresh=None, ia=None, oa=None, ra=None, wa=None):
    return {
        "input_cost_per_million": inp, "output_cost_per_million": out,
        "cache_read_cost_per_million": cr, "cache_write_cost_per_million": cw,
        "cache_write_1h_cost_per_million": cw1h, "request_cost": req,
        "source": source, "source_url": url, "pricing_version": ver,
        "fetched_at": fetched, "tier_threshold_tokens": thresh,
        "input_cost_per_million_above": ia, "output_cost_per_million_above": oa,
        "cache_read_cost_per_million_above": ra, "cache_write_cost_per_million_above": wa,
    }


def U(inp=0, out=0, cr=0, cw=0, reason=0, req=1, raw=None, ttl=None):
    return {"input_tokens": inp, "output_tokens": out, "cache_read_tokens": cr,
            "cache_write_tokens": cw, "reasoning_tokens": reason, "request_count": req,
            "raw_usage": raw, "cache_write_by_ttl": ttl or []}


def ROUTE(provider="openai", model="gpt-x", base_url="", mode="official_docs_snapshot"):
    return {"provider": provider, "model": model, "base_url": base_url, "billing_mode": mode}


US = 1700000000000000          # 2023-11-14T22:13:20Z (before deadline)
US_LATE = 1900000000000000     # after 2027-03-01T08:00:00Z
DEADLINE = 1803888000000000

# 1. _snap
F = "_snap"
add(F, {"inp": "5.00", "out": "25.00", "cache_read": "0.50", "cache_write": "6.25", "version": "v1", "url": "http://x"})
add(F, {"inp": "2.50", "out": "10.00", "version": "v2"})
add(F, {"inp": "1.00", "out": "6.00", "cache_read": "0.10", "version": "v3", "url": None,
        "tiers": {"tier_threshold_tokens": 272000, "input_cost_per_million_above": dec("20.00"),
                  "output_cost_per_million_above": dec("75.00")}})
add(F, {"inp": "bad", "out": "1", "version": "v"})
add(F, {"inp": "5", "out": "nope", "version": "v"})
add(F, {"inp": None, "out": "1", "version": "v"})
add(F, {"inp": "5", "out": "1", "version": "v", "tiers": {"bogus": 1}})
add(F, {"inp": "1_0.5_0", "out": "2e1", "cache_read": " 0.5 ", "version": "v"})
add(F, {"inp": "5", "out": "1", "cache_write": "x", "version": "v"})
add(F, {"inp": "0.00", "out": "0", "version": "v"})

# 2. _usage_field
F = "_usage_field"
add(F, {"obj": {"a": {"b": 7}}, "path": ["a", "b"]})
add(F, {"obj": {"a": -3}, "path": ["a"]})
add(F, {"obj": {"a": " 12 "}, "path": ["a"]})
add(F, {"obj": {"a": "1__0"}, "path": ["a"]})
add(F, {"obj": {}, "path": ["a"]})
add(F, {"obj": {"a": None}, "path": ["a", "b"]})
add(F, {"obj": 5, "path": ["a"]})
add(F, {"obj": {"a": 5.7}, "path": ["a"]})
add(F, {"obj": {"a": True}, "path": ["a"]})
add(F, {"obj": {"a": "1_000"}, "path": ["a"]})
add(F, {"obj": [1, 2], "path": ["a"]})
add(F, {"obj": {"a": {"b": "0x10"}}, "path": ["a", "b"]})
add(F, {"obj": {"a": 0}, "path": ["a"]})

# 3. _first_nonzero
F = "_first_nonzero"
add(F, {"obj": {"x": 0, "y": 5}, "paths": [["x"], ["y"]]})
add(F, {"obj": {"x": 3, "y": 5}, "paths": [["x"], ["y"]]})
add(F, {"obj": {"x": 0, "y": 0}, "paths": [["x"], ["y"]]})
add(F, {"obj": {"x": 1}, "paths": []})
add(F, {"obj": {"d": {"n": 9}}, "paths": [["d", "n"], ["x"]]})
add(F, {"obj": {}, "paths": [["a"], ["b"]]})

# 4. _strip_prefix
F = "_strip_prefix"
add(F, {"name": "us.anthropic.claude", "prefixes": ["global.", "us."]})
add(F, {"name": "global.anthropic.claude", "prefixes": ["global.", "us."]})
add(F, {"name": "anthropic.claude", "prefixes": ["global.", "us."]})
add(F, {"name": "us.x", "prefixes": []})
add(F, {"name": "", "prefixes": ["us."]})
add(F, {"name": "us.", "prefixes": ["us."]})

# 5. with_served_service_tier
F = "with_served_service_tier"
add(F, {"usage": U(raw={"a": 1}), "response": {"service_tier": " Priority "}})
add(F, {"usage": U(raw={"a": 1}), "response": {"service_tier": "  "}})
add(F, {"usage": U(raw={"a": 1}), "response": {"service_tier": 5}})
add(F, {"usage": U(raw={"a": 1}), "response": None})
add(F, {"usage": U(raw=None), "response": {"service_tier": "fast"}})
add(F, {"usage": U(raw={"service_tier": "old", "b": 2}), "response": {"service_tier": "new"}})
add(F, {"usage": None, "response": {"service_tier": "fast"}})
add(F, {"usage": U(raw={}), "response": {"service_tier": "default"}})

# 6. _served_openai_tier
F = "_served_openai_tier"
add(F, {"usage": U(raw={"service_tier": "fast"})})
add(F, {"usage": U(raw={"x": 1})})
add(F, {"usage": U(raw=None)})
add(F, {"usage": U(raw=[1, 2])})
add(F, {"usage": U(raw={"service_tier": None})})
add(F, {"usage": U(raw={"service_tier": 7})})

# 7. _openai_served_tier_entry
F = "_openai_served_tier_entry"
add(F, {"model": "gpt-6-astra", "tier": "ultrafast"})
add(F, {"model": "gpt-6-luna", "tier": "ultrafast"})
add(F, {"model": "gpt-6-astra", "tier": "priority"})
add(F, {"model": "gpt-6.2-astra", "tier": "priority"})
add(F, {"model": "gpt-6.2-sol", "tier": "ultrafast", "io": {"utc_now": US}})
add(F, {"model": "gpt-6.2-astra", "tier": "ultrafast", "io": {"utc_now": US_LATE}})
add(F, {"model": "gpt-6.2-sol", "tier": None, "io": {"utc_now": US}})
add(F, {"model": "gpt-6.2-astra", "tier": "priority", "io": {"utc_now": R("RuntimeError", "clock")}})
add(F, {"model": "gpt-6.2-sol", "tier": "ultrafast", "io": {"utc_now": DEADLINE}})
add(F, {"model": "gpt-6.2-astra", "tier": "ultrafast", "io": {}})  # fail-closed utc_now

# 8. _served_fast
F = "_served_fast"
add(F, {"usage": U(raw={"speed": "fast"})})
add(F, {"usage": U(raw={"speed": "slow"})})
add(F, {"usage": U(raw={"x": 1})})
add(F, {"usage": U(raw=None)})
add(F, {"usage": U(raw=[1])})
add(F, {"usage": U(raw={"speed": None})})

# 9. _anthropic_fast_mode_entry
F = "_anthropic_fast_mode_entry"
add(F, {"model": "claude-opus-4-8"})
add(F, {"model": "claude-opus-5"})
add(F, {"model": "claude-opus-5-5"})
add(F, {"model": "claude-fable-5-2"})
add(F, {"model": "claude-mythos-5-2"})
add(F, {"model": "claude-opus-4.8", "io": {"normalize_anthropic_model_name": "claude-opus-4-8"}})
add(F, {"model": "unknown", "io": {"normalize_anthropic_model_name": "unknown"}})
add(F, {"model": "claude-opus-4.8", "io": {"normalize_anthropic_model_name": R("ValueError", "norm")}})
add(F, {"model": "claude-opus-4.8", "io": {"normalize_anthropic_model_name": None}})
add(F, {"model": "claude-opus-4.8", "io": {}})  # fail-closed normalizer
add(F, {"model": "claude-opus-4.8", "io": {"normalize_anthropic_model_name": R("SystemExit", "bye")}})

# 10. _models_dev_pricing_entry
F = "_models_dev_pricing_entry"
MI = {"has_cost_data": True, "cost_input": "0.000002", "cost_output": "0.00001",
      "cost_cache_read": None, "cost_cache_write": None}
TD4 = [dec("2.000000"), dec("10.000000"), None, None]
MD_IO = {"get_model_info": MI, "to_decimal": TD4, "utc_now": US}
add(F, {"route": ROUTE(), "io": MD_IO})
add(F, {"route": ROUTE(base_url="https://api.openai.com/v1"),
        "io": dict(MD_IO, base_url_origin=L(["https", "api.openai.com", 443]))})
add(F, {"route": ROUTE(base_url="http://proxy.com"),
        "io": dict(MD_IO, base_url_origin=L(["http", "proxy.com", 80]))})
add(F, {"route": ROUTE(base_url="https://proxy.com"),
        "io": dict(MD_IO, base_url_origin=L(["https", "proxy.com", 443]))})
add(F, {"route": ROUTE(base_url="https://api.openai.com"),
        "io": dict(MD_IO, base_url_origin=L(["https", "api.openai.com", 8443]))})
add(F, {"route": ROUTE(base_url="https://sub.api.openai.com"),
        "io": dict(MD_IO, base_url_origin=L(["https", "sub.api.openai.com", 443]))})
add(F, {"route": ROUTE(provider="unknownprov"), "io": MD_IO})
add(F, {"route": ROUTE(model=""), "io": MD_IO})
add(F, {"route": ROUTE(), "io": dict(MD_IO, get_model_info=None)})
add(F, {"route": ROUTE(), "io": dict(MD_IO, get_model_info={"has_cost_data": False, "cost_input": "1", "cost_output": "1", "cost_cache_read": None, "cost_cache_write": None})})
add(F, {"route": ROUTE(), "io": dict(MD_IO, to_decimal=[R("ValueError", "dec"), dec("1"), None, None])})
add(F, {"route": ROUTE(), "io": dict(MD_IO, utc_now=R("RuntimeError", "clock"))})
add(F, {"route": ROUTE(base_url="https://x"), "io": dict(MD_IO, base_url_origin=None)})
add(F, {"route": ROUTE(base_url="https://x"), "io": dict(MD_IO, base_url_origin=L(["https", "x"]))})
add(F, {"route": ROUTE(), "io": {}})  # fail-closed get_model_info
add(F, {"route": ROUTE(provider="anthropic", model="claude-x"), "io": MD_IO})
add(F, {"route": ROUTE(), "io": dict(MD_IO, get_model_info=R("KeyboardInterrupt", "ki"))})

# 11. cache_write_tiers
F = "cache_write_tiers"
add(F, {"response_usage": {"cache_creation": {"ephemeral_5m_input_tokens": 10, "ephemeral_1h_input_tokens": 40}}})
add(F, {"response_usage": {"input_tokens_details": {"cache_write_tokens_details": {"ttl_30m_tokens": 5, "ttl_12h_tokens": 7}}}})
add(F, {"response_usage": {"cache_creation": {"ephemeral_5m_input_tokens": 0, "ephemeral_1h_input_tokens": 0}}})
add(F, {"response_usage": {}})
add(F, {"response_usage": {"cache_creation": {"ephemeral_5m_input_tokens": 3}}})
add(F, {"response_usage": 5})
add(F, {"response_usage": {"cache_creation": {"ephemeral_5m_input_tokens": "8"}, "input_tokens_details": {"cache_write_tokens_details": {"ttl_30m_tokens": 2}}}})

# 12. _cache_write_buckets
F = "_cache_write_buckets"
E_OFF = E(inp=dec("5.00"), cw=dec("6.25"), source="official_docs_snapshot", ver="v", ia=dec("10.00"))
E_API = E(inp=dec("5.00"), cw=dec("6.25"), cw1h=dec("10.00"), source="provider_models_api", ver="v")
E_API_ZERO = E(inp=dec("5.00"), cw=dec("6.25"), cw1h=dec("0.00"), source="provider_models_api", ver="v")
E_NOINPUT = E(cw=dec("6.25"), source="official_docs_snapshot", ver="v")
E_NOABOVE = E(inp=dec("5.00"), cw=dec("6.25"), source="official_docs_snapshot", ver="v")
add(F, {"usage": U(cw=100, ttl=[["1h", 40]]), "entry": E_API})
add(F, {"usage": U(cw=100, ttl=[["1h", 40]]), "entry": E_API_ZERO})
add(F, {"usage": U(cw=100, ttl=[]), "entry": E_OFF})
add(F, {"usage": U(cw=100, ttl=[["1h", 40]]), "entry": E_NOINPUT})
add(F, {"usage": U(cw=100, ttl=[["1h", 40]]), "entry": E_OFF})
add(F, {"usage": U(cw=100, ttl=[["1h", 40]]), "entry": E_NOABOVE})
add(F, {"usage": U(cw=10, ttl=[["1h", 40]]), "entry": E_OFF})
add(F, {"usage": U(cw=0, ttl=[["1h", 0]]), "entry": E_OFF})
add(F, {"usage": U(cw=100, ttl=[["5m", 20], ["1h", 40]]), "entry": E_API})
add(F, {"usage": U(cw=100, ttl=[["1h", 0]]), "entry": E_API})

# 13. _unknown_cost
F = "_unknown_cost"
add(F, {"source": "none", "notes": []})
add(F, {"source": "official_docs_snapshot", "notes": ["fast-mode pricing unavailable for model"]})
add(F, {"source": "none", "notes": ["a", "b"]})
add(F, {"source": "provider_models_api", "notes": ["x"]})
add(F, {"source": "none"})

# 14. host
F = "host"
add(F, {"url": "https://api.openai.com", "name": "openai.com", "io": {"base_url_host_matches": True}})
add(F, {"url": "https://x.com", "name": "openai.com", "io": {"base_url_host_matches": False}})
add(F, {"url": "https://x.com", "name": "openai.com", "io": {"base_url_host_matches": R("ValueError", "hm")}})
add(F, {"url": "https://x.com", "name": "openai.com", "io": {}})  # fail-closed
add(F, {"url": None, "name": "openai.com", "io": {"base_url_host_matches": False}})
add(F, {"url": "https://x.com", "name": "openai.com", "io": {"base_url_host_matches": R("SystemExit", "bye")}})

# 15. rate
F = "rate"
RP = {"pricing": {"prompt": "0.000002"}, "key": "prompt", "aliases": [], "model_id": "m1"}
add(F, dict(RP), {"to_decimal": dec("0.000002")}, {"rejected": []})
add(F, dict(RP, pricing={"prompt": "-1"}), {"to_decimal": dec("-1"), "logger.debug": None}, {"rejected": []})
add(F, dict(RP, pricing={"prompt": "NaN"}), {"to_decimal": dec("NaN"), "logger.debug": None}, {"rejected": []})
add(F, dict(RP, pricing={"prompt": "Infinity"}), {"to_decimal": dec("Infinity"), "logger.debug": None}, {"rejected": []})
add(F, dict(RP, pricing={"prompt": None, "cached_prompt": "0.5"}, aliases=["cached_prompt", "input_cache_read"]),
    {"to_decimal": dec("0.5")}, {"rejected": []})
add(F, dict(RP, pricing={"prompt": None}), {"to_decimal": None}, {"rejected": []})
add(F, dict(RP), {"to_decimal": R("ValueError", "dec")}, {"rejected": []})
add(F, dict(RP, pricing={"prompt": "-1"}), {"to_decimal": dec("-1"), "logger.debug": R("RuntimeError", "log")}, {"rejected": []})
add(F, dict(RP), {}, {"rejected": []})  # fail-closed to_decimal
add(F, dict(RP), {"to_decimal": R("SystemExit", "bye")}, {"rejected": []})
add(F, dict(RP, pricing={"prompt": "-1"}), {"to_decimal": dec("-1"), "logger.debug": None}, {"rejected": ["prompt", "a"]})
add(F, dict(RP, pricing={"prompt": "0"}), {"to_decimal": dec("0")}, {"rejected": []})

# 16. per_million
F = "per_million"
add(F, dict(RP), {"to_decimal": dec("0.000002")}, {"rejected": []})
add(F, dict(RP, pricing={"prompt": None}), {"to_decimal": None}, {"rejected": []})
add(F, dict(RP, pricing={"prompt": "-1"}), {"to_decimal": dec("-1"), "logger.debug": None}, {"rejected": []})
add(F, dict(RP), {"to_decimal": R("ValueError", "dec")}, {"rejected": []})
add(F, dict(RP), {}, {"rejected": []})  # fail-closed
add(F, dict(RP, pricing={"prompt": None, "cached_prompt": "0.000003"}, aliases=["cached_prompt"]),
    {"to_decimal": dec("0.000003")}, {"rejected": []})
add(F, dict(RP), {"to_decimal": R("KeyboardInterrupt", "ki")}, {"rejected": []})
add(F, dict(RP, pricing={"prompt": "0.125"}), {"to_decimal": dec("0.125")}, {"rejected": []})


def write_fixtures():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=True, separators=(",", ":")) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for fn in sorted(counts):
        flag = "" if counts[fn] >= 5 else "  <-- BELOW 5"
        print("  %-30s %d%s" % (fn, counts[fn], flag))
    assert all(v >= 5 for v in counts.values()), "a func has <5 cases"
    assert len(counts) == 16, "expected 16 funcs"


def check_tables():
    import importlib.util
    spec = importlib.util.spec_from_file_location("up2_oracle_chk", str(HERE / "oracle_usage_pricing_flow.py"))
    o = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(o)
    o.load_ns()
    ns = o.NS

    def entryj(e):
        d = o.norm_value(e)
        d["fetched_at"] = None  # tables built at import have fetched_at None
        return d

    live = {
        "ULTRA": {m: entryj(e) for m, e in ns["_OPENAI_ULTRAFAST_PRICING"].items()},
        "GPT62": {m: entryj(ns["_OFFICIAL_DOCS_PRICING"][("openai", m)]) for m in ("gpt-6.2-astra", "gpt-6.2-sol")},
        "FAST": {m: entryj(e) for m, e in ns["_ANTHROPIC_FAST_MODE_PRICING"].items()},
        "HOSTS": dict(ns["_MODELS_DEV_DIRECT_HOSTS"]),
        "TIERPATHS": [[t, list(p)] for t, p in ns["_CACHE_WRITE_TIER_PATHS"]],
    }
    text = PORT_C.read_text(encoding="utf-8")
    drift = 0
    for name, obj in live.items():
        m = re.search(r"/\*UP2_TABLE_BEGIN:%s\*/(.*?)/\*UP2_TABLE_END:%s\*/" % (name, name), text, re.S)
        if not m:
            print("CHECK %s: marker MISSING" % name)
            drift += 1
            continue
        block = m.group(1)
        lm = re.search(r'=\s*"(.*)";', block, re.S)
        if not lm:
            print("CHECK %s: literal MISSING" % name)
            drift += 1
            continue
        lit = lm.group(1).encode("utf-8").decode("unicode_escape")
        try:
            cobj = json.loads(lit)
        except Exception as ex:
            print("CHECK %s: literal parse FAIL %s" % (name, ex))
            drift += 1
            continue
        if cobj != obj:
            print("CHECK %s: DRIFT" % name)
            print("  C   :", json.dumps(cobj, sort_keys=True)[:300])
            print("  LIVE:", json.dumps(obj, sort_keys=True)[:300])
            drift += 1
        else:
            print("CHECK %s: OK (%d keys)" % (name, len(cobj) if isinstance(cobj, dict) else len(cobj)))
    print("TABLE CHECK: %d drift" % drift)
    return drift


if __name__ == "__main__":
    if "--check" in sys.argv:
        sys.exit(1 if check_tables() else 0)
    write_fixtures()
