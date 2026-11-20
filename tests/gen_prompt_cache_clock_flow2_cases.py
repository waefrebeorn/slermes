#!/usr/bin/env python3
"""Generate tests/fixtures/prompt_cache_clock_flow2.jsonl (>=5 cases per each
of the 40 UNCLAIMED prompt_cache_clock.py defs). Emits {"func","in"} lines;
the harness adds "out" and the oracle diffs C vs LIVE under the same seams."""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "prompt_cache_clock_flow2.jsonl"
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def EV(**kw):
    d = {"route": ["p", "http://x", "m"], "destination": ["p", "http://x", "m"],
         "route_class": "anthropic_spec", "requested_ttl": "5m", "sent_at": 100,
         "read_tokens": 0, "write_tokens": 0, "tier": "5m", "confidence": "proven",
         "reason": "r", "coverage": {"5m": 400}, "guaranteed_until": 400,
         "estimated_until": 400, "session_id": "s", "generation": 0, "upstream": None}
    d.update(kw)
    return d


def VD(tiers, conf, reason):
    return {"tiers": tiers, "confidence": conf, "reason": reason}


# ── _list_field ─────────────────────────────────────────────────────────────
add("_list_field", api_kwargs={"messages": [1, 2, 3]}, key="messages")
add("_list_field", api_kwargs={"messages": "notlist"}, key="messages")
add("_list_field", api_kwargs={}, key="messages")
add("_list_field", api_kwargs={"messages": None}, key="messages")
add("_list_field", api_kwargs={"messages": []}, key="messages")
add("_list_field", api_kwargs={"tools": [{"a": 1}]}, key="tools")

# ── record_attempt_sent ─────────────────────────────────────────────────────
for i, io in enumerate([
    {"attempt_facts": {"sent_at": 1, "marker_ttl": "5m"}},
    {"attempt_facts": None},
    {"attempt_facts": {"sent_at": 2}},
    {},
    {"attempt_facts": {"$seq": [{"sent_at": 3}, None]}},
]):
    add("record_attempt_sent", agent={}, api_kwargs={"k": i}, io=io)

# ── _refresh_restart ────────────────────────────────────────────────────────
for tier, at in [("5m", 100), ("1h", 0), ("12h", 5), ("30m", 50), ("bogus", 10)]:
    add("_refresh_restart", deadline=0, tier=tier, at=at)

# ── _refresh_openai ─────────────────────────────────────────────────────────
for deadline, at in [(0, 100), (1000, 50), (100, 1), (0, 0), (5000, 900)]:
    add("_refresh_openai", deadline=deadline, tier="30m", at=at)

# ── _managed_unload_end ─────────────────────────────────────────────────────
for i, managed in enumerate([True, False, True, False, True]):
    add("_managed_unload_end", base_url=f"http://x{i}", idle_from=100 + i,
        io={"is_managed_endpoint": managed},
        env={"IDLE_UNLOAD_S": 600, "_SWEEP_EVERY_S": 30})

# ── _route_key ──────────────────────────────────────────────────────────────
for i, url in enumerate(["http://x", "https://y:1234", "", "custom://z", "http://LOCAL"]):
    add("_route_key", provider=["anthropic", None, 5, "x", ""][i], base_url=url,
        model=["claude", "gpt", None, 7, ""][i],
        io={"normalize_route_base_url": url.lower()})

# ── response_upstream ───────────────────────────────────────────────────────
for resp in [{"provider": "openrouter"}, {"id": "chatcmpl-abc"}, {"id": "gen-1"},
             {"provider": "", "id": "xyz-1"}, {"provider": None, "id": None}, {"id": "123-abc"}]:
    add("response_upstream", response=resp)

# ── upstream_switched ───────────────────────────────────────────────────────
for b, a in [("x", "y"), ("x", "x"), (None, "y"), ("x", None), (None, None), ("", "y"), ("x", "")]:
    add("upstream_switched", before=b, after=a)

# ── _display_route ──────────────────────────────────────────────────────────
for i in range(5):
    add("_display_route", agent={"provider": "anthropic", "base_url": "http://x", "model": "m" + str(i)},
        io={"normalize_route_base_url": ["http://x", "http://y", "", "http://z", "http://w"][i]})

# ── _compression_count ──────────────────────────────────────────────────────
for cc, rw in [(3, 2), (0, 0), (5, None), (None, 4), ("7", True), (2, "x")]:
    add("_compression_count", agent={"context_compressor": {"compression_count": cc},
                                     "_session_db": {"context_rewrites": rw}})

# ── _storage_errors ─────────────────────────────────────────────────────────
for i in range(5):
    add("_storage_errors", tag=i)

# ── _clock_store ────────────────────────────────────────────────────────────
for i, handle in enumerate([{"usage": {"u": 1}}, None, {"usage": None}, {"usage": {"u": 2}}, None]):
    add("_clock_store", agent={"_session_db": {"db": i}}, io={"agent_interface": handle})

# ── _stored_clock ───────────────────────────────────────────────────────────
for i, env in enumerate([
    {"has_prompt_cache_clock": True, "prompt_cache_clock_callable": True},
    {"has_prompt_cache_clock": True, "prompt_cache_clock_callable": False},
    {"has_get_prompt_cache_clock": True, "get_prompt_cache_clock_callable": True},
    {"has_get_prompt_cache_clock": True, "get_prompt_cache_clock_callable": False},
    {},
]):
    add("_stored_clock", store={}, env=env)

# ── _agent_generation ───────────────────────────────────────────────────────
for i, (h, g) in enumerate([(None, None), ({"usage": {}}, 5), (None, 3),
                            ({"usage": {}}, None), ({"usage": {}}, True)]):
    add("_agent_generation", agent={"session_id": "s" + str(i), "_session_db": {"db": 1}},
        io={"agent_interface": h, "current_generation": g})

# ── _agent_invalid_reason ───────────────────────────────────────────────────
for i, (gen, mark, cc, reason) in enumerate([
    (None, 0, 0, None), (None, 0, 5, "compacted"), (3, 0, 0, None),
    (3, 0, 0, "route_changed"), (None, 2, 2, None),
]):
    add("_agent_invalid_reason",
        agent={"session_id": "s", "_prompt_cache_compression_mark": mark,
               "context_compressor": {"compression_count": cc},
               "provider": "anthropic", "base_url": "http://x", "model": "m"},
        event=EV(), generation=gen,
        env={"normalize_route_base_url": "http://x"},
        io={"normalize_route_base_url": "http://x", "_invalid_reason": reason})

# ── context_witness ─────────────────────────────────────────────────────────
for i, g in enumerate([None, 2, 0, 5, 1]):
    add("context_witness", agent={"session_id": "s" + str(i), "_session_db": {"db": 1},
                                  "provider": "anthropic", "base_url": "http://x", "model": "m",
                                  "context_compressor": {"compression_count": i}},
        io={"agent_interface": None, "current_generation": g, "normalize_route_base_url": "http://x"})

# ── _set_event ──────────────────────────────────────────────────────────────
for i, mark in enumerate([None, 7, 0, 3, None]):
    add("_set_event", agent={"context_compressor": {"compression_count": i}},
        event=EV(confidence="proven"), mark=mark)

# ── _valid_previous ─────────────────────────────────────────────────────────
for i, (conf, reason) in enumerate([("proven", None), ("proven", "compacted"),
                                    ("unknown", None), ("declared", None), ("proven", "route_changed")]):
    add("_valid_previous", agent={"_prompt_cache_clock": EV(confidence=conf), "session_id": "s",
                                  "provider": "anthropic", "base_url": "http://x", "model": "m",
                                  "context_compressor": {"compression_count": 0}},
        generation=0, env={"restored_flag": True, "normalize_route_base_url": "http://x"},
        io={"normalize_route_base_url": "http://x", "_invalid_reason": reason})

# ── _moa_destination ────────────────────────────────────────────────────────
for client in [{}, {"last_aggregator_route": {"model": "m"}},
               {"last_aggregator_route": {"model": "m"}, "last_aggregator_attempt": {"marker_ttl": "5m"}},
               {"last_aggregator_route": "x"}, {"last_aggregator_route": {}, "last_aggregator_attempt": "y"}]:
    add("_moa_destination", agent={"client": client})

# ── _learn ──────────────────────────────────────────────────────────────────
for i, prev in enumerate([None, EV(upstream="a"), EV(upstream="b"), EV(upstream=None), EV(upstream="a")]):
    add("_learn", previous=prev, provider="anthropic", base_url="http://x", model="m",
        attempt={"marker_ttl": "5m", "replay": bool(i % 2)},
        read=1, write=0, at=200, upstream=["a", "a", "b", None, "a"][i], session_id="s",
        io={"observe": None})

# ── _declared_tier ──────────────────────────────────────────────────────────
for i, (rc, verdict) in enumerate([
    ("anthropic_spec", VD(["5m"], "declared", "anthropic_requested")),
    ("anthropic_spec", VD(["5m", "1h"], "declared", "anthropic_requested")),
    ("openai_spec", VD(["30m"], "declared", "openai_spec_30m")),
    ("anthropic_spec", VD(["1h"], "proven", "anthropic_usage_split")),
    ("openai_spec", VD(["5m"], "declared", "openai_write_tier")),
]):
    add("_declared_tier", route_class=rc, requested_ttl=["5m", "1h", None, "1h", None][i],
        write_tokens=0, provider="anthropic", base_url="http://x",
        io={"route_ttl_ceiling": None,
            "rule_anthropic_spec": VD(["5m"], "declared", "anthropic_requested") if rc == "anthropic_spec" else None,
            "rule_openai_spec": VD(["30m"], "declared", "openai_spec_30m") if rc == "openai_spec" else None,
            "$unused": i})

# ── declared_write_tier ─────────────────────────────────────────────────────
for i, rt in enumerate(["5m", "1h", None, "30m", "12h"]):
    add("declared_write_tier", event=EV(requested_ttl=rt, write_tokens=1),
        io={"route_ttl_ceiling": None,
            "rule_anthropic_spec": VD(["5m"], "declared", "anthropic_requested")})

# ── request_write_tier ──────────────────────────────────────────────────────
for i, facts in enumerate([{"marker_ttl": "5m", "sent_at": 1}, {"openai_ttl": "30m"}, None,
                           {"marker_ttl": "1h"}, {"retention": "24h"}]):
    add("request_write_tier", api_kwargs={"prompt_cache_options": {}}, usage={"cache_write_tokens": 1},
        provider="anthropic", base_url="http://x", api_mode="messages", model="m",
        io={"attempt_facts": facts, "classify_route": "anthropic_spec", "route_ttl_ceiling": None,
            "rule_anthropic_spec": VD(["5m"], "declared", "anthropic_requested")})

# ── _learn_silent_miss ──────────────────────────────────────────────────────
for i in range(6):
    prev = EV(destination=["anthropic", "http://x", "m"], confidence=["proven", "unknown", "proven",
              "proven", "proven", "proven"][i], read_tokens=10)
    add("_learn_silent_miss",
        agent={"_prompt_cache_clock": prev, "session_id": "s", "_session_db": {"db": 1},
               "provider": "anthropic", "base_url": "http://x", "model": "m",
               "context_compressor": {"compression_count": 0}, "client": {}},
        usage={"prompt_tokens": [100, 100, 2, 100, 100, 100][i]},
        model="m", provider="anthropic", base_url="http://x", attempt={"sent_at": 5},
        io={"normalize_route_base_url": "http://x", "agent_interface": None,
            "current_generation": 0, "_invalid_reason": None, "observe": None},
        env={"MIN_CACHEABLE_PREFIX_TOKENS": 50})

# ── record_cache_response ───────────────────────────────────────────────────
for i in range(7):
    add("record_cache_response",
        agent={"session_id": "s", "_session_db": {"db": 1}, "context_compressor": {"compression_count": 0},
               "provider": "anthropic", "base_url": "http://x", "model": "m", "client": {}},
        usage={"cache_read_tokens": [10, 10, 0, 10, 10, 10, 10][i],
               "cache_write_tokens": [0, 1, 0, 0, 0, 0, 0][i]},
        model="m", provider="anthropic", base_url="http://x", api_mode="messages",
        attempt={"marker_ttl": "5m", "sent_at": 100, "retention": None},
        witness=["s", 0, 0, ["anthropic", "http://x", "m"]], upstream=None,
        env={"restored_flag": True, "normalize_route_base_url": "http://x"},
        io={"observe": None, "normalize_route_base_url": "http://x",
            "classify_route": "anthropic_spec", "route_ttl_ceiling": None,
            "rule_anthropic_spec": VD(["5m"], "declared", "anthropic_requested"),
            "time": 200,
            "_fold_coverage": [{"5m": 400}, False],
            "_ends": [400, 400],
            "_estimated_end": 400,
            "_confidence": ["declared", "anthropic_requested"]})

# ── record_unobserved_cache_response ────────────────────────────────────────
for i in range(5):
    add("record_unobserved_cache_response",
        agent={"session_id": "s" + str(i), "_session_db": {"db": 1}, "provider": "anthropic",
               "base_url": "http://x", "model": "m", "context_compressor": {"compression_count": 0}},
        usage={"cache_read_tokens": [1, 0, 5, 1, 0][i], "cache_write_tokens": 0},
        reason=["codex_app_server", "other", "x", "y", "z"][i],
        io={"normalize_route_base_url": "http://x", "agent_interface": None,
            "current_generation": i, "time": 100 + i})

# ── event_to_json ───────────────────────────────────────────────────────────
for i, kw in enumerate([{}, {"tier": None}, {"coverage": {"5m": 400}},
                        {"confidence": "unknown"}, {"session_id": "other", "generation": 2}]):
    add("event_to_json", event=EV(**kw))

# ── _fallback_event ─────────────────────────────────────────────────────────
for i in range(6):
    add("_fallback_event",
        agent={"provider": "anthropic", "base_url": "http://x", "model": "m", "session_id": "s",
               "api_mode": "messages", "_use_prompt_caching": bool(i % 2), "_cache_ttl": "5m"},
        last_assistant_at=100 + i, generation=i,
        env={"use_prompt_caching": bool(i % 2), "cache_ttl_auto": False, "normalize_route_base_url": "http://x"},
        io={"effective_cache_ttl": "5m" if i % 2 else None, "classify_route": "anthropic_spec",
            "route_ttl_ceiling": None, "normalize_route_base_url": "http://x",
            "rule_anthropic_spec": VD(["5m"], "declared", "anthropic_requested"),
            "rule_openai_spec": VD(["30m"], "declared", "openai_spec_30m")})

# ── restore_cache_clock ─────────────────────────────────────────────────────
for i in range(7):
    add("restore_cache_clock",
        agent={"session_id": ["s", None, "s", "s", "s", "s", "s"][i], "_session_db": {"db": 1},
               "provider": "anthropic", "base_url": "http://x", "model": "m",
               "context_compressor": {"compression_count": 0}},
        env={"has_prompt_cache_clock": [False, True, True, True, True, True, True][i],
             "prompt_cache_clock_callable": True, "use_prompt_caching": False,
             "normalize_route_base_url": "http://x"},
        io={"agent_interface": {"usage": {}},
            "reader": [None, {"clock": json.dumps(EV())}, {"clock": "notjson"},
                       {"last_assistant_at": 100, "generation": 1}, {}, {"clock": None},
                       {"clock": json.dumps(EV(confidence="unknown"))}][i],
            "event_from_json": None,
            "classify_route": "anthropic_spec", "route_ttl_ceiling": None,
            "normalize_route_base_url": "http://x",
            "rule_anthropic_spec": VD(["5m"], "declared", "anthropic_requested"),
            "effective_cache_ttl": None,
            "time": 100})

# ── _unknown ────────────────────────────────────────────────────────────────
for reason in ["session_rotated", "compacted", "route_changed", "storage_unavailable", "x"]:
    add("_unknown", reason=reason, event=EV(sent_at=123))

# ── _window ─────────────────────────────────────────────────────────────────
for i, kw in enumerate([{"confidence": "unknown"}, {"estimated_until": None, "guaranteed_until": None},
                        {"confidence": "proven", "estimated_until": 500, "guaranteed_until": 400},
                        {"confidence": "declared", "estimated_until": 500},
                        {"confidence": "assumed", "guaranteed_until": 300}]):
    add("_window", event=EV(**kw))

# ── cache_window ────────────────────────────────────────────────────────────
for i, conf in enumerate(["proven", "unknown", "declared", "assumed", "proven"]):
    add("cache_window",
        agent={"_prompt_cache_clock": EV(confidence=conf, estimated_until=500, guaranteed_until=400),
               "session_id": "s", "_session_db": {"db": 1}, "provider": "anthropic",
               "base_url": "http://x", "model": "m", "context_compressor": {"compression_count": 0}},
        env={"restored_flag": True, "normalize_route_base_url": "http://x"},
        io={"agent_interface": None, "current_generation": 0,
            "normalize_route_base_url": "http://x", "_invalid_reason": None})

# ── _saved_route_changed ────────────────────────────────────────────────────
for i, saved in enumerate([["p", "http://x", "m"], ["p", "http://y", "m"],
                           ["p", "http://x", "other"], ["custom", "http://y", "m"], [None, None, None]]):
    add("_saved_route_changed", event=EV(route=["p", "http://x", "m"]), saved=saved,
        io={"normalize_route_base_url": saved[1] or ""})

# ── stored_cache_window ─────────────────────────────────────────────────────
for i, reason in enumerate([None, "compacted", "route_changed", "session_rotated", None]):
    add("stored_cache_window", raw_clock=json.dumps(EV()), session_id=["s", "s", "s", "other", "s"][i],
        generation=0, saved_route=["p", "http://x", "m"],
        io={"event_from_json": EV(), "_invalid_reason": reason,
            "normalize_route_base_url": "http://x"})

# ── cache_window_payload ────────────────────────────────────────────────────
for i in range(5):
    add("cache_window_payload",
        agent={"_prompt_cache_clock": EV(confidence="proven", estimated_until=500, guaranteed_until=400),
               "session_id": "s", "_session_db": {"db": 1}, "provider": "anthropic",
               "base_url": "http://x", "model": "m", "context_compressor": {"compression_count": 0},
               "_use_prompt_caching": True},
        env={"restored_flag": True, "use_prompt_caching": True, "normalize_route_base_url": "http://x"},
        io={"agent_interface": None, "current_generation": 0, "normalize_route_base_url": "http://x",
            "_invalid_reason": None, "rule_for": "rule", "display_rule": "learned-rule",
            "prompt_cache_warming_disabled": False, "tier_pending": False})

# ── _field ──────────────────────────────────────────────────────────────────
for i, name in enumerate(["estimated_until", "confidence", "tier", "missing", "guaranteed_until"]):
    add("_field", window={"estimated_until": 5, "confidence": "proven"}, name=name)

# ── _clock ──────────────────────────────────────────────────────────────────
for i in range(5):
    add("_clock", ts=100 + i, io={"localtime": [2026, 1, 1, 0, 0, i, 0, 0, 0],
                                  "strftime": f"12:{10 + i:02d}"})

# ── _cache_line ─────────────────────────────────────────────────────────────
for i, (conf, warm, cold) in enumerate([
    ("unknown", False, False), ("proven", True, False), ("proven", False, True),
    ("declared", False, False), ("proven", True, True), ("assumed", False, False),
]):
    add("_cache_line",
        window={"confidence": conf, "reason": "route_changed", "tier": "5m",
                "guaranteed_until": 400, "estimated_until": 500},
        estimated=500 if i != 0 else None, now=300,
        io={"cache_certainly_warm": warm, "cache_certainly_cold": cold,
            "t": "translated", "localtime": [2026, 1, 1, 0, 0, 0, 0, 0, 0],
            "strftime": "12:00", "_duration": "5m"})

# ── render_prompt_cache_line ────────────────────────────────────────────────
for i in range(6):
    add("render_prompt_cache_line",
        window={"confidence": "proven", "estimated_until": 500, "guaranteed_until": 400,
                "tier": "5m", "reason": "r",
                "tier_pending": bool(i & 1), "learned": ["L"][0] if i & 2 else None,
                "warming_off": bool(i & 4)},
        now=300,
        io={"t": "line", "cache_certainly_warm": True, "cache_certainly_cold": False,
            "localtime": [2026, 1, 1, 0, 0, 0, 0, 0, 0], "strftime": "12:00", "_duration": "2m"})

# ── prompt_cache_is_cold ────────────────────────────────────────────────────
for i, (conf, reason, cold) in enumerate([
    ("proven", "r", True), ("declared", "r", False), ("assumed", "r", True),
    ("proven", "route_changed", False), ("unknown", "r", True), ("proven", "r", False),
]):
    add("prompt_cache_is_cold",
        agent={"_prompt_cache_clock": EV(confidence=conf, reason=reason, estimated_until=500,
                                         guaranteed_until=400),
               "session_id": "s", "_session_db": {"db": 1}, "provider": "anthropic",
               "base_url": "http://x", "model": "m", "context_compressor": {"compression_count": 0}},
        now=600,
        env={"restored_flag": True, "normalize_route_base_url": "http://x"},
        io={"agent_interface": None, "current_generation": 0,
            "normalize_route_base_url": "http://x", "_invalid_reason": None,
            "cache_certainly_cold": cold})


OUT.parent.mkdir(parents=True, exist_ok=True)
# Flow2 fixtures use env.restored_flag to model the agent's already-restored
# attribute; mirror it onto the agent double the LIVE driver builds.
for c in CASES:
    env = c["in"].get("env") or {}
    if env.get("restored_flag"):
        ag = c["in"].get("agent")
        if isinstance(ag, dict):
            ag["_prompt_cache_clock_restored"] = True
with OUT.open("w", encoding="utf-8", newline="\n") as fh:
    for c in CASES:
        fh.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")

# report per-func counts
from collections import Counter
cnt = Counter(c["func"] for c in CASES)
names = {c["func"] for c in CASES}
print(f"wrote {len(CASES)} cases across {len(names)} functions -> {OUT}")
mins = [(f, cnt[f]) for f in sorted(names)]
print("min per func:", min(n for _, n in mins))
bad = [f for f, n in mins if n < 5]
if bad:
    print("BELOW 5:", bad)
    raise SystemExit(1)
