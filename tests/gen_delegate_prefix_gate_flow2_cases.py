#!/usr/bin/env python3
"""Generate tests/fixtures/delegate_prefix_gate_flow2.jsonl.

One JSON object per line: {"func": <live_name>, "in": {...}} with `in.io`
holding the universal-seam scripts and `in.state` the mutable carriers. Both
the C harness and the live oracle consume the SAME fixture. >=5 cases per func.
"""
import json
import os
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "delegate_prefix_gate_flow2.jsonl"

MARK = "receiving stream response"
cases = []


def child(cid, **attrs):
    return {"__attr__": dict(attrs), "__id__": cid}


def case(func, **inp):
    cases.append({"func": func, "in": inp})


# ---------------- _route_key ----------------
case("_route_key", state={"child": child("c", provider="p", base_url="b", model="m", api_mode="chat")})
case("_route_key", state={"child": child("c", provider="p", model="m")})
case("_route_key", state={"child": child("c", provider=None, base_url=None, model=None, api_mode=None)})
case("_route_key", state={"child": child("c", provider="p", base_url="b", model=7, api_mode=False)})
case("_route_key", state={"child": child("c")})
case("_route_key", state={"child": {"provider": "p"}})  # not an attribute double -> all None

# ---------------- _caches_prompts ----------------
for i, val in enumerate([True, False, None, 1, "true"]):
    at = {"provider": "p"}
    if i != 2:
        at["_use_prompt_caching"] = val
    case("_caches_prompts", state={"child": child("c", **at)})
case("_caches_prompts", state={"child": child("c")})

# ---------------- _first_response_started ----------------
case("_first_response_started", state={"child": child("c", session_api_calls=0, _last_activity_desc="idle")})
case("_first_response_started", state={"child": child("c", session_api_calls=1, _last_activity_desc="idle")})
case("_first_response_started", state={"child": child("c", session_api_calls=2, _last_activity_desc="idle")})
case("_first_response_started", state={"child": child("c", session_api_calls=0, _last_activity_desc=MARK)})
case("_first_response_started", state={"child": child("c", session_api_calls=None, _last_activity_desc=MARK)})
case("_first_response_started", state={"child": child("c", session_api_calls=0)})
case("_first_response_started", state={"child": child("c", _last_activity_desc="other")})

# ---------------- _stop_requested ----------------
for i, val in enumerate([True, False, None, 1, "yes"]):
    at = {"provider": "p"}
    if i != 2:
        at["_interrupt_requested"] = val
    case("_stop_requested", state={"child": child("c", **at)})
case("_stop_requested", state={"child": child("c")})

# ---------------- PrefixCacheGate.__init__ ----------------
case("PrefixCacheGate.__init__", leader=child("L", provider="p", base_url="b", model="m", api_mode="chat"), max_wait_s=1.5)
case("PrefixCacheGate.__init__", leader=child("L"), max_wait_s=0)
case("PrefixCacheGate.__init__", leader=child("L", provider=None, model="m"), max_wait_s=10)
case("PrefixCacheGate.__init__", leader={"provider": "p"}, max_wait_s=2)
case("PrefixCacheGate.__init__", leader=child("L", provider="p", model="m"), max_wait_s=-1)

# ---------------- PrefixCacheGate.for_children ----------------
c_full = child("c0", provider="p", base_url="b", model="m", api_mode="chat", _use_prompt_caching=True)
c_follow = child("c1", provider="p", base_url="b", model="m", api_mode="chat", _use_prompt_caching=True)
case("PrefixCacheGate.for_children", children=[[0, "t0", c_full]], max_wait_s=2)
case("PrefixCacheGate.for_children", children=[[0, "t0", c_full], [1, "t1", c_follow]], max_wait_s=0)
case("PrefixCacheGate.for_children", children=[[0, "t0", c_full], [1, "t1", c_follow]], max_wait_s=-1)
case("PrefixCacheGate.for_children",
     children=[[0, "t0", child("c0", provider="p", _use_prompt_caching=False)], [1, "t1", c_follow]], max_wait_s=2)
case("PrefixCacheGate.for_children",
     children=[[0, "t0", c_full], [1, "t1", c_follow], [2, "t2", c_follow]], max_wait_s=0.5)
case("PrefixCacheGate.for_children", children=[], max_wait_s=2)

# gate helper: a gate-state dict
def gate(leader, route, mw=0.5, done=False):
    return {"leader": leader, "max_wait_s": mw, "leader_route": route, "leader_done": done}


route_full = ["p", "b", "m", "chat"]
leader = child("L", provider="p", base_url="b", model="m", api_mode="chat", _use_prompt_caching=True)

# ---------------- PrefixCacheGate.follows ----------------
case("PrefixCacheGate.follows", state={"gate": gate(leader, route_full)},
     child=child("L", provider="p", base_url="b", model="m", api_mode="chat", _use_prompt_caching=True))
case("PrefixCacheGate.follows", state={"gate": gate(leader, route_full)},
     child=child("C", provider="p", base_url="b", model="m", api_mode="chat", _use_prompt_caching=True))
case("PrefixCacheGate.follows", state={"gate": gate(leader, route_full)},
     child=child("C", provider="p", base_url="b", model="m", api_mode="chat", _use_prompt_caching=False))
case("PrefixCacheGate.follows", state={"gate": gate(leader, route_full)},
     child=child("C", provider="p", base_url="X", model="m", api_mode="chat", _use_prompt_caching=True))
case("PrefixCacheGate.follows", state={"gate": gate(leader, [None, None, "m", None], mw=1)},
     child=child("C", provider=None, base_url=None, model="m", api_mode=None, _use_prompt_caching=True))
case("PrefixCacheGate.follows", state={"gate": gate(leader, route_full, done=True)},
     child=child("C", provider="p", base_url="b", model="m", api_mode="chat", _use_prompt_caching=True))

# ---------------- PrefixCacheGate.run ----------------
# leader: run() then finally leader_done.set()
case("PrefixCacheGate.run", state={"gate": gate(leader, route_full)},
     child=child("L", _use_prompt_caching=True), run={"__callable__": "gate.run_fn"},
     io={"gate.run_fn": "done"})
case("PrefixCacheGate.run", state={"gate": gate(leader, route_full)},
     child=child("L", _use_prompt_caching=True), run={"__callable__": "gate.run_fn"},
     io={"gate.run_fn": {"__raise__": "RuntimeError", "message": "boom"}})
# non-follower runs immediately
case("PrefixCacheGate.run", state={"gate": gate(leader, route_full)},
     child=child("C", provider="p", base_url="X", model="m", api_mode="chat", _use_prompt_caching=True),
     run={"__callable__": "gate.run_fn"}, io={"gate.run_fn": "ran"})
# follower, gate already done -> wait returns true, then run
case("PrefixCacheGate.run", state={"gate": gate(leader, route_full, done=True)},
     child=child("C", provider="p", base_url="b", model="m", api_mode="chat", _use_prompt_caching=True),
     run={"__callable__": "gate.run_fn"}, io={"time.monotonic": 5, "gate.run_fn": "ran"})
# follower, stop requested -> _wait false -> no run
case("PrefixCacheGate.run", state={"gate": gate(leader, route_full)},
     child=child("C", provider="p", base_url="b", model="m", api_mode="chat",
                 _use_prompt_caching=True, _interrupt_requested=True),
     run={"__callable__": "gate.run_fn"}, io={"time.monotonic": 5})
# follower, leader first-response started -> wait true immediately, then run
leader_started = child("L", provider="p", base_url="b", model="m", api_mode="chat",
                       _use_prompt_caching=True, session_api_calls=1)
case("PrefixCacheGate.run", state={"gate": gate(leader_started, route_full)},
     child=child("C", provider="p", base_url="b", model="m", api_mode="chat", _use_prompt_caching=True),
     run={"__callable__": "gate.run_fn"}, io={"time.monotonic": 5, "gate.run_fn": "ran"})
# follower, parent stop requested
case("PrefixCacheGate.run", state={"gate": gate(leader, route_full)},
     child=child("C", provider="p", base_url="b", model="m", api_mode="chat", _use_prompt_caching=True),
     parent=child("P", _interrupt_requested=True),
     run={"__callable__": "gate.run_fn"}, io={"time.monotonic": 5})

# ---------------- PrefixCacheGate._wait ----------------
# leader_done already set
case("PrefixCacheGate._wait", state={"gate": gate(leader, route_full, done=True)},
     child=child("C"), io={"time.monotonic": 5})
# leader first response started
case("PrefixCacheGate._wait", state={"gate": gate(leader_started, route_full)},
     child=child("C"), io={"time.monotonic": 5})
# child stop
case("PrefixCacheGate._wait", state={"gate": gate(leader, route_full)},
     child=child("C", _interrupt_requested=True), io={"time.monotonic": 5})
# parent stop
case("PrefixCacheGate._wait", state={"gate": gate(leader, route_full)},
     child=child("C"), parent=child("P", _interrupt_requested=True), io={"time.monotonic": 5})
# deadline elapsed (two clock reads, no event.wait)
case("PrefixCacheGate._wait", state={"gate": gate(leader, route_full, mw=0.5)},
     child=child("C"), io={"time.monotonic": [0, 1]})
# one poll then deadline (clock reads 0, 0.1, 1.2; one event.wait)
case("PrefixCacheGate._wait", state={"gate": gate(leader, route_full, mw=1)},
     child=child("C"), io={"time.monotonic": [0, 0.1, 1.2], "event.wait": None})
# scripted clock raise propagates
case("PrefixCacheGate._wait", state={"gate": gate(leader, route_full, mw=1)},
     child=child("C"), io={"time.monotonic": {"__raise__": "RuntimeError", "message": "clock"}})

OUT.parent.mkdir(parents=True, exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
    for c in cases:
        fh.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")

# assertion: >=5 cases per function
from collections import Counter
cnt = Counter(c["func"] for c in cases)
assert all(cnt[f] >= 5 for f in [
    "_route_key", "_caches_prompts", "_first_response_started", "_stop_requested",
    "PrefixCacheGate.__init__", "PrefixCacheGate.for_children",
    "PrefixCacheGate.follows", "PrefixCacheGate.run", "PrefixCacheGate._wait",
]), cnt
print("wrote %d cases to %s" % (len(cases), OUT))
print(dict(cnt))
