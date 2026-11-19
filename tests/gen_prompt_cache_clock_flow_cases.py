#!/usr/bin/env python3
"""Generate tests/fixtures/prompt_cache_clock_flow.jsonl for the
agent/prompt_cache_clock.py FLOW oracle. Emits {"func","in"} case lines (the
harness adds "out"; the oracle diffs C vs LIVE under the same scripts). Every
ported function gets >=5 cases; TTL/clock thresholds carry ==, >, < boundary
cases. _marker_ttl fixtures keep a UNIQUE minimum TTL seconds so set-iteration
order never decides the result. current_generation reader scripts are
int/bool/str/null only (libjson cannot distinguish int from float)."""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "prompt_cache_clock_flow.jsonl"
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def EV(**kw):
    base = {"route": ["p", "http://x", "m"], "destination": ["p", "http://x", "m"],
            "route_class": "anthropic_spec", "requested_ttl": "5m", "sent_at": 100,
            "read_tokens": 0, "write_tokens": 0, "tier": "5m", "confidence": "proven",
            "reason": "r"}
    base.update(kw)
    return base


def V(tiers, conf, reason):
    return {"tiers": tiers, "confidence": conf, "reason": reason}


def CTX(**kw):
    base = {"route_class": "anthropic_spec", "requested_ttl": "5m", "retention": None,
            "write_tiers": {}, "write_tokens": 0, "ceiling": None}
    base.update(kw)
    return base


# ── _block_marker_ttls ──────────────────────────────────────────────────────
add("_block_marker_ttls", block={"cache_control": {"ttl": "1h"}}, out=[])
add("_block_marker_ttls", block={"content": [{"cache_control": {"ttl": "5m"}},
                                             {"cache_control": "ephemeral"}]}, out=["1h"])
add("_block_marker_ttls", block="x", out=["a"])
add("_block_marker_ttls", block={"cache_control": None}, out=[])
add("_block_marker_ttls", block={"cache_control": {"ttl": 0}}, out=[])
add("_block_marker_ttls", block={"cache_control": {"ttl": ""}}, out=["5m"])
add("_block_marker_ttls", block={"content": "notlist", "cache_control": {"ttl": "30m"}}, out=[])
add("_block_marker_ttls", block={"cache_control": {"ttl": None}}, out=[])
add("_block_marker_ttls", block={"content": [{"cache_control": {"ttl": "1h"}}, "str",
                                             {"cache_control": {"ttl": "5m"}}]}, out=[])

# ── _marker_ttl ─────────────────────────────────────────────────────────────
add("_marker_ttl", api_kwargs={"system": [{"cache_control": {"ttl": "1h"}}],
                               "messages": [{"cache_control": {"ttl": "5m"}}],
                               "tools": [{"cache_control": {"ttl": "30m"}}]})
add("_marker_ttl", api_kwargs={"messages": [{"content": "x"}]})
add("_marker_ttl", api_kwargs={"system": [{"cache_control": {"ttl": "1h"}}]})
add("_marker_ttl", api_kwargs={"messages": [{"cache_control": {"ttl": "1h"}}, {}, {}, {},
                                            {"cache_control": {"ttl": "30m"}}, {}]})
add("_marker_ttl", api_kwargs={"tools": [{"cache_control": {"ttl": "1h"}},
                                         {"cache_control": {"ttl": "5m"}}]})
add("_marker_ttl", api_kwargs={"system": [{"cache_control": {"ttl": 0}}]})
add("_marker_ttl", api_kwargs={"system": [{"cache_control": {"ttl": "xx"}},
                                          {"cache_control": {"ttl": "5m"}}]})
add("_marker_ttl", api_kwargs={"messages": [{"cache_control": {"ttl": "12h"}}]})
add("_marker_ttl", api_kwargs={})

# ── _wire_field ─────────────────────────────────────────────────────────────
add("_wire_field", api_kwargs={"k": "top"}, key="k")
add("_wire_field", api_kwargs={"extra_body": {"k": "extra"}}, key="k")
add("_wire_field", api_kwargs={"extra_body": "notdict"}, key="k")
add("_wire_field", api_kwargs={"k": None, "extra_body": {"k": "extra"}}, key="k")
add("_wire_field", api_kwargs={}, key="k")
add("_wire_field", api_kwargs={"k": "top", "extra_body": {"k": "extra"}}, key="k")
add("_wire_field", api_kwargs={"extra_body": {"k": 5}}, key="k")

# ── attempt_facts ───────────────────────────────────────────────────────────
add("attempt_facts", api_kwargs={"prompt_cache_options": {"ttl": "30m"},
                                 "prompt_cache_retention": "24h",
                                 "system": [{"cache_control": {"ttl": "1h"}}]},
    io={"time": 1000})
add("attempt_facts", api_kwargs=5, io={"time": 1000})
add("attempt_facts", api_kwargs={}, io={"time": 5})
add("attempt_facts", api_kwargs={"prompt_cache_options": "notdict"}, io={"time": 7})
add("attempt_facts", api_kwargs={"extra_body": {"prompt_cache_retention": "24h"}}, io={"time": 9})
add("attempt_facts", api_kwargs={}, io={"time": {"__raise__": "OSError", "msg": "clock"}})
add("attempt_facts", api_kwargs={"prompt_cache_options": {"ttl": "1h"},
                                 "system": [{"cache_control": {"ttl": "5m"}}]}, io={"time": 2})

# ── classify_route ──────────────────────────────────────────────────────────
add("classify_route", provider="anthropic", base_url="http://x", api_mode="messages",
    model="claude", has_markers=True, retention=None)
add("classify_route", provider="openai", base_url="http://x", api_mode="responses",
    model="gpt", has_markers=False, retention=None)
add("classify_route", provider="openai", base_url="http://x", api_mode="responses",
    model="gpt", has_markers=True, retention="24h")
add("classify_route", provider="custom", base_url="http://y", api_mode="chat",
    model="m", has_markers=False, retention="24h")
add("classify_route", provider="nous", base_url="http://z", api_mode="messages",
    model="claude", has_markers=True, retention=None)
add("classify_route", provider="deepseek", base_url="http://d", api_mode="chat",
    model="ds", has_markers=False, retention=None)

# ── _written_tiers ──────────────────────────────────────────────────────────
add("_written_tiers", tiers={"1h": 2, "5m": 1, "30m": 0, "bogus": 3, "12h": 1})
add("_written_tiers", tiers={})
add("_written_tiers", tiers={"5m": 0})
add("_written_tiers", tiers={"30m": 1, "5m": 1})
add("_written_tiers", tiers={"bogus": 5})
add("_written_tiers", tiers={"12h": 1, "1h": 1, "30m": 1, "5m": 1})
add("_written_tiers", tiers={"5m": True})

# ── _rule_anthropic_spec ────────────────────────────────────────────────────
add("_rule_anthropic_spec", ctx=CTX())
add("_rule_anthropic_spec", ctx=CTX(ceiling="1h"))
add("_rule_anthropic_spec", ctx=CTX(requested_ttl="1h", ceiling="30m"))
add("_rule_anthropic_spec", ctx=CTX(write_tiers={"1h": 5}, write_tokens=5, ceiling="30m"))
add("_rule_anthropic_spec", ctx=CTX(write_tiers={"5m": 1}, write_tokens=1))
add("_rule_anthropic_spec", ctx=CTX(ceiling="bogus"))
add("_rule_anthropic_spec", ctx=CTX(write_tiers={"5m": 1, "1h": 2}, write_tokens=3, ceiling="1h"))
add("_rule_anthropic_spec", ctx=CTX(requested_ttl=None))
add("_rule_anthropic_spec", ctx=CTX(requested_ttl="1h", ceiling="1h"))

# ── _rule_openai_spec ───────────────────────────────────────────────────────
add("_rule_openai_spec", ctx=CTX(route_class="openai_spec"))
add("_rule_openai_spec", ctx=CTX(route_class="openai_spec", write_tiers={"12h": 3}, write_tokens=3))
add("_rule_openai_spec", ctx=CTX(route_class="openai_spec", write_tiers={"5m": 0}))
add("_rule_openai_spec", ctx=CTX(route_class="openai_spec", write_tiers={"30m": 1, "1h": 1}, write_tokens=2))
add("_rule_openai_spec", ctx=CTX(route_class="openai_spec", write_tiers={"bogus": 1}, write_tokens=1))
add("_rule_openai_spec", ctx=CTX(route_class="openai_spec", write_tiers={}))

# ── _fold_coverage ──────────────────────────────────────────────────────────
add("_fold_coverage", previous=EV(coverage={"5m": 100, "1h": 200}), verdict=V(["5m"], "declared", "r"),
    route_class="anthropic_spec", at=100, read=1, write=0)
add("_fold_coverage", previous=EV(coverage={"1h": 200}), verdict=V(["30m"], "declared", "r"),
    route_class="openai_spec", at=100, read=1, write=0)
add("_fold_coverage", previous=EV(coverage={"5m": 500}), verdict=V(["5m"], "declared", "r"),
    route_class="unobserved", at=100, read=1, write=1)
add("_fold_coverage", previous=None, verdict=V(["5m"], "declared", "r"),
    route_class="anthropic_spec", at=100, read=0, write=1)
add("_fold_coverage", previous=EV(coverage={"30m": 5000}), verdict=V(["30m"], "declared", "r"),
    route_class="openai_spec", at=100, read=1, write=1)
add("_fold_coverage", previous=EV(coverage={"5m": 500}), verdict=V(["5m"], "declared", "r"),
    route_class="anthropic_spec", at=100, read=0, write=1)
add("_fold_coverage", previous=EV(coverage={"5m": 50}), verdict=V(["5m"], "declared", "r"),
    route_class="anthropic_spec", at=100, read=1, write=0)
add("_fold_coverage", previous=None, verdict=V(["bogus"], "declared", "r"),
    route_class="anthropic_spec", at=100, read=0, write=1)
add("_fold_coverage", previous=EV(coverage={"5m": 101}), verdict=V(["5m"], "declared", "r"),
    route_class="anthropic_spec", at=100, read=1, write=0)

# ── _ends ───────────────────────────────────────────────────────────────────
add("_ends", coverage={})
add("_ends", coverage={"5m": 400})
add("_ends", coverage={"5m": 400, "1h": 3700})
add("_ends", coverage={"1h": 3700, "5m": 400})
add("_ends", coverage={"30m": 1900, "12h": 43300, "5m": 400})
add("_ends", coverage={"5m": -5})

# ── _estimated_end ──────────────────────────────────────────────────────────
EST_ENV = {"idle_unload_s": 600, "sweep_every_s": 30}
add("_estimated_end", coverage={}, base_url="http://x", retention=None, at=100,
    previous=None, carried=False, idle_from=100, env=EST_ENV, io={"is_local_endpoint": False})
add("_estimated_end", coverage={"5m": 400}, base_url="http://x", retention=None, at=100,
    previous=None, carried=False, idle_from=100, env=EST_ENV, io={"is_local_endpoint": False})
add("_estimated_end", coverage={"5m": 400}, base_url="http://127/managed", retention=None, at=100,
    previous=None, carried=False, idle_from=150, env=EST_ENV,
    io={"is_local_endpoint": True, "is_managed_endpoint": True})
add("_estimated_end", coverage={"5m": 400}, base_url="http://127/user", retention=None, at=100,
    previous=None, carried=False, idle_from=150, env=EST_ENV,
    io={"is_local_endpoint": True, "is_managed_endpoint": False})
add("_estimated_end", coverage={"5m": 400}, base_url="http://x", retention="24h", at=100,
    previous=None, carried=False, idle_from=100, env=EST_ENV, io={"is_local_endpoint": False})
add("_estimated_end", coverage={"5m": 400}, base_url="http://x", retention=None, at=100,
    previous=EV(estimated_until=99999), carried=True, idle_from=100, env=EST_ENV,
    io={"is_local_endpoint": False})
add("_estimated_end", coverage={"5m": 400}, base_url="http://x", retention=24, at=100,
    previous=None, carried=False, idle_from=100, env=EST_ENV, io={"is_local_endpoint": False})
add("_estimated_end", coverage={"5m": 400}, base_url="http://x", retention="1h", at=100,
    previous=None, carried=False, idle_from=100, env=EST_ENV, io={"is_local_endpoint": False})
add("_estimated_end", coverage={"5m": 400}, base_url="http://x", retention=None, at=100,
    previous=None, carried=False, idle_from=100, env=EST_ENV,
    io={"is_local_endpoint": {"__raise__": "OSError", "msg": "net"}})
add("_estimated_end", coverage={"5m": 400}, base_url="http://127/managed", retention=None, at=200,
    previous=None, carried=False, idle_from=150, env=EST_ENV,
    io={"is_local_endpoint": True, "is_managed_endpoint": True})

# ── _confidence ─────────────────────────────────────────────────────────────
add("_confidence", verdict=V(["5m"], "declared", "vr"), previous=None, carried=False, write=1)
add("_confidence", verdict=V(["5m"], "declared", "vr"), previous=None, carried=True, write=1)
add("_confidence", verdict=V(["5m"], "proven", "vr"), previous=EV(confidence="assumed", reason="pr"),
    carried=True, write=0)
add("_confidence", verdict=V(["5m"], "declared", "vr"), previous=EV(confidence="proven", reason="pr"),
    carried=True, write=1)
add("_confidence", verdict=V(["5m"], "proven", "vr"), previous=EV(confidence="assumed", reason="pr"),
    carried=True, write=1)
add("_confidence", verdict=V(["5m"], "declared", "vr"), previous=EV(confidence="declared", reason="pr"),
    carried=True, write=1)
add("_confidence", verdict=V(["5m"], "bogus", "vr"), previous=EV(), carried=True, write=1)
add("_confidence", verdict=V(["5m"], "assumed", "vr"), previous=EV(confidence="unknown", reason="pr"),
    carried=True, write=1)

# ── _invalid_reason ─────────────────────────────────────────────────────────
add("_invalid_reason", event=EV(session_id="a"), session_id="b", generation=None, route=None)
add("_invalid_reason", event=EV(generation=-1), session_id="s", generation=0, route=None)
add("_invalid_reason", event=EV(generation=0), session_id="s", generation=-1, route=None)
add("_invalid_reason", event=EV(generation=None), session_id="s", generation=0, route=None)
add("_invalid_reason", event=EV(generation=1), session_id="s", generation=2, route=None)
add("_invalid_reason", event=EV(), session_id="s", generation=None, route=["x", "http://x", "m"])
add("_invalid_reason", event=EV(session_id="s", generation=0), session_id="s", generation=0,
    route=["p", "http://x", "m"])
add("_invalid_reason", event=EV(session_id=""), session_id="b", generation=None, route=None)
add("_invalid_reason", event=EV(session_id="a", generation=-1), session_id="b", generation=0, route=None)
add("_invalid_reason", event=EV(generation=0), session_id="s", generation=None, route=None)

# ── current_generation ──────────────────────────────────────────────────────
add("current_generation", session_id="s", env={"has_reader": True}, io={"prompt_cache_generation": 3})
add("current_generation", session_id="s", env={"has_reader": False})
add("current_generation", session_id="", env={"has_reader": True}, io={"prompt_cache_generation": 3})
add("current_generation", session_id=None, env={"has_reader": True}, io={"prompt_cache_generation": 3})
add("current_generation", session_id="s", env={"has_reader": True}, io={"prompt_cache_generation": "x"})
add("current_generation", session_id="s", env={"has_reader": True}, io={"prompt_cache_generation": True})
add("current_generation", session_id="s", env={"has_reader": True},
    io={"prompt_cache_generation": {"__raise__": "OSError", "msg": "db"}})
add("current_generation", session_id="s", env={"has_reader": True},
    io={"prompt_cache_generation": {"__raise__": "ValueError", "msg": "bad"}})
add("current_generation", session_id="s", env={"has_reader": True},
    io={"prompt_cache_generation": {"__raise__": "sqlite3.Error", "msg": "sql"}})
add("current_generation", session_id="s", env={"has_reader": True},
    io={"prompt_cache_generation": {"__raise__": "PersistenceError", "msg": "p"}})
add("current_generation", session_id=5, env={"has_reader": True}, io={"prompt_cache_generation": 7})
add("current_generation", session_id="s", env={"has_reader": True},
    io={"prompt_cache_generation": {"__raise__": "WorkerPersistenceError", "msg": "w"}})

# ── event_from_json ─────────────────────────────────────────────────────────
FULL = EV(read_tokens=1, write_tokens=2)
add("event_from_json", raw=json.dumps(FULL))
add("event_from_json", raw=json.dumps(dict(FULL, route_class="openai_ttl")))
add("event_from_json", raw=json.dumps({k: v for k, v in FULL.items() if k != "route"}))
add("event_from_json", raw=json.dumps({k: v for k, v in FULL.items() if k != "reason"}))
add("event_from_json", raw=json.dumps(dict(FULL, bogus=1)))
add("event_from_json", raw=json.dumps(dict(FULL, route="ab")))
add("event_from_json", raw=json.dumps(dict(FULL, route=5)))
add("event_from_json", raw="{bad")
add("event_from_json", raw=5)
add("event_from_json", raw="[1]")
add("event_from_json", raw=json.dumps({k: v for k, v in FULL.items() if k != "route_class"}))
add("event_from_json", raw=json.dumps({k: v for k, v in FULL.items()
                                       if k not in ("coverage", "guaranteed_until", "estimated_until",
                                                    "session_id", "generation", "upstream")}))
add("event_from_json", raw=json.dumps({k: v for k, v in FULL.items() if k != "destination"}))
add("event_from_json", raw=json.dumps(dict(FULL, route_class="anthropic_native")))
add("event_from_json", raw="null")
add("event_from_json", raw=json.dumps(dict(FULL, coverage={"5m": 400}, session_id="s", generation=2)))

# ── cache_certainly_warm ────────────────────────────────────────────────────
add("cache_certainly_warm", window={"guaranteed_until": 100}, now=99)
add("cache_certainly_warm", window={"guaranteed_until": 100}, now=100)
add("cache_certainly_warm", window={"guaranteed_until": 100}, now=101)
add("cache_certainly_warm", window={"guaranteed_until": None}, now=99)
add("cache_certainly_warm", window=None, now=99)
add("cache_certainly_warm", window={"guaranteed_until": 100}, now=None, io={"time": 50})
add("cache_certainly_warm", window={"guaranteed_until": "100"}, now=50)
add("cache_certainly_warm", window={"guaranteed_until": "abc"}, now=50)
add("cache_certainly_warm", window={"guaranteed_until": 100}, now=None,
    io={"time": {"__raise__": "OSError", "msg": "clock"}})
add("cache_certainly_warm", window={"guaranteed_until": 100}, now=99, env={"as_object": True})
add("cache_certainly_warm", window={"guaranteed_until": []}, now=50)
add("cache_certainly_warm", window={"guaranteed_until": 100.5}, now=100.25)

# ── cache_certainly_cold ────────────────────────────────────────────────────
add("cache_certainly_cold", window={"confidence": "proven", "estimated_until": 100}, now=100)
add("cache_certainly_cold", window={"confidence": "proven", "estimated_until": 100}, now=99)
add("cache_certainly_cold", window={"confidence": "proven", "estimated_until": 100}, now=101)
add("cache_certainly_cold", window={"confidence": "unknown", "estimated_until": 100}, now=200)
add("cache_certainly_cold", window={"confidence": "proven", "estimated_until": None}, now=200)
add("cache_certainly_cold", window=None, now=200)
add("cache_certainly_cold", window={"confidence": "proven", "estimated_until": 100}, now=None,
    io={"time": 100})
add("cache_certainly_cold", window={"confidence": "proven", "estimated_until": "abc"}, now=50)
add("cache_certainly_cold", window={"confidence": None, "estimated_until": 100}, now=100)
add("cache_certainly_cold", window={"confidence": "declared", "estimated_until": 100}, now=100,
    env={"as_object": True})
add("cache_certainly_cold", window={"confidence": "proven", "estimated_until": 100}, now=None,
    io={"time": {"__raise__": "RuntimeError", "msg": "x"}})

# ── _duration ───────────────────────────────────────────────────────────────
for s in [0, 59, 59.9, 60, 61, 3599, 3600, 3661, 7200, -5, "90", None, "x", 86400, True, 3660]:
    add("_duration", seconds=s)


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    # per-function case counts (>=5 required)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"wrote {len(CASES)} cases to {OUT}")
    for fn, n in sorted(counts.items()):
        flag = "" if n >= 5 else "  <-- UNDER 5"
        print(f"  {fn}: {n}{flag}")


if __name__ == "__main__":
    main()