#!/usr/bin/env python3
"""Generator for tests/fixtures/auxiliary_client_flow3.jsonl.

>=5 cases per ported function of the UNCLAIMED remainder ported as
src/agent/auxiliary_client_flow3.c (guard/accumulator leaves).

Case shape: {"func": name, "in": {...args..., "self": {...}, "io": {seam: script}}}
Script shapes (shared by C ac3_call and the oracle):
  - absent key            -> None
  - {"$seq": [...]}       -> per-call queue (last repeats; exhaustion raises RuntimeError)
  - {"__raise__": cls, "msg": m} -> raises a real exception of that class
  - any other JSON value  -> reused for every call
Domain: self event fields are {"set": bool}; callable fields are
{"__callable__": marker[, "begin": bool]}; integral floats collapse to ints on
both sides; logger args carrying an exception project to {"__raise__","msg"}.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "auxiliary_client_flow3.jsonl")
CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def seq(*vals):
    return {"$seq": list(vals)}


def raise_(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


def ev(on=False):
    return {"set": on}


def cc(marker="cc", begin=False):
    d = {"__callable__": marker}
    if begin:
        d["begin"] = True
    return d


def base_self(**over):
    d = {
        "hard_deadline": 50, "_progress_deadline": 40,
        "_deadline_lock": {}, "_attempt_stream_lock": {},
        "_protected_cancel_check": None,
        "_attempt_stream": None, "no_progress_timeout": 30,
        "_start": 0, "saw_content": ev(False), "timed_out": ev(False),
        "timeout_release_pending": ev(False), "stream_finished": ev(False),
        "_client": {"id": "client"}, "_owner_tid": 1, "total_timeout": None,
        "_timer": None,
    }
    d.update(over)
    return d


QUIET_IO = {"aux_interrupt_cancel_requested": False, "is_interrupted": False,
            "time.monotonic": 0}


def close_io(**over):
    d = {
        "begin_timeout_cleanup": True, "threading.get_ident": 1,
        "close_quietly": None, "attempt_stream_socket": None,
        "shutdown_socket": None, "force_close_tcp_sockets": 0,
        "evict_cached_client_instance": None,
    }
    d.update(over)
    return d


# ── effective_deadline (5) ─────────────────────────────────────────────────
for i, (h, p) in enumerate([(10, 20), (20, 10), (10, 10), (5.5, 3.2), (0, 0)]):
    case("effective_deadline", tag=i,
         self=base_self(hard_deadline=h, _progress_deadline=p))

# ── cancel_requested (5) ───────────────────────────────────────────────────
case("cancel_requested", self=base_self(_protected_cancel_check=None))
case("cancel_requested", self=base_self(_protected_cancel_check=cc()),
     io={"captured_aux_cancel_requested": True})
case("cancel_requested", self=base_self(_protected_cancel_check=cc()),
     io={"captured_aux_cancel_requested": False})
case("cancel_requested", self=base_self(_protected_cancel_check=cc()),
     io={"captured_aux_cancel_requested": {"v": 1}})
case("cancel_requested", self=base_self(_protected_cancel_check=cc()),
     io={"captured_aux_cancel_requested": raise_("ValueError", "boom")})

# ── adopt_stream (5) ───────────────────────────────────────────────────────
for i, s in enumerate([None, "stream-x", {"id": "s"}, 7, True]):
    case("adopt_stream", tag=i, self=base_self(), stream=s)

# ── release_stream (5) ─────────────────────────────────────────────────────
case("release_stream", self=base_self(), stream=None, io={"close_quietly": None})
case("release_stream", self=base_self(_attempt_stream={"id": "s"}),
     stream={"id": "s"}, io={"close_quietly": None})
case("release_stream", self=base_self(_attempt_stream="s"), stream="s",
     io={"close_quietly": raise_("RuntimeError", "x")})
case("release_stream", self=base_self(_attempt_stream="s"), stream="s",
     io={"close_quietly": raise_("SystemExit", "bye")})
case("release_stream", self=base_self(_attempt_stream=5), stream=5,
     io={"close_quietly": None})

# ── close_attempt_stream (6) ───────────────────────────────────────────────
case("close_attempt_stream", self=base_self(_attempt_stream=None), failure_note="note")
case("close_attempt_stream", self=base_self(_attempt_stream="s"), failure_note="owner",
     io={"threading.get_ident": 1, "close_quietly": None})
case("close_attempt_stream", self=base_self(_attempt_stream="s"), failure_note="no-sock",
     io={"threading.get_ident": 2, "attempt_stream_socket": None, "close_quietly": None})
case("close_attempt_stream", self=base_self(_attempt_stream="s"), failure_note="sock",
     io={"threading.get_ident": 2, "attempt_stream_socket": "sock",
         "shutdown_socket": None})
case("close_attempt_stream", self=base_self(_attempt_stream="s"), failure_note="raise",
     io={"threading.get_ident": 2, "attempt_stream_socket": "sock",
         "shutdown_socket": raise_("ValueError", "v")})
case("close_attempt_stream", self=base_self(_attempt_stream="s"), failure_note="cq",
     io={"threading.get_ident": 1, "close_quietly": raise_("RuntimeError", "c")})

# ── record_progress (5) ────────────────────────────────────────────────────
for i, (m, npt) in enumerate([(100, 5), (0, 0), (1.5, 2.5), (10, 3)]):
    case("record_progress", tag=i,
         self=base_self(no_progress_timeout=npt), io={"time.monotonic": m})
case("record_progress", self=base_self(no_progress_timeout=1),
     io={"time.monotonic": raise_("ValueError", "t")})

# ── timeout_message (5) ────────────────────────────────────────────────────
case("timeout_message", self=base_self(_start=0, hard_deadline=15),
     io={"time.monotonic": seq(10, 20)})
case("timeout_message", self=base_self(_start=0, hard_deadline=100, saw_content=ev(False)),
     io={"time.monotonic": seq(5, 6)})
case("timeout_message", self=base_self(_start=0, hard_deadline=100, saw_content=ev(True)),
     io={"time.monotonic": seq(5, 6)})
case("timeout_message", self=base_self(), io={"time.monotonic": raise_("RuntimeError", "a")})
case("timeout_message", self=base_self(), io={"time.monotonic": seq(1, raise_("ValueError", "b"))})

# ── _close_client_on_timeout (7) ───────────────────────────────────────────
case("_close_client_on_timeout",
     self=base_self(_protected_cancel_check=cc(begin=True)),
     io=close_io(begin_timeout_cleanup=True, **{"threading.get_ident": 1}))
case("_close_client_on_timeout",
     self=base_self(_protected_cancel_check=cc(begin=True), _attempt_stream=None),
     io=close_io(begin_timeout_cleanup=False, **{"threading.get_ident": 1}))
case("_close_client_on_timeout",
     self=base_self(_protected_cancel_check=cc(begin=False), _attempt_stream=None),
     io=close_io(captured_aux_cancel_requested=True, **{"threading.get_ident": 1}))
case("_close_client_on_timeout",
     self=base_self(_protected_cancel_check=cc(begin=True), _attempt_stream=None),
     io=close_io(begin_timeout_cleanup=True, **{"threading.get_ident": 2},
                 force_close_tcp_sockets=2))
case("_close_client_on_timeout",
     self=base_self(_protected_cancel_check=cc(begin=True), _attempt_stream=None),
     io=close_io(begin_timeout_cleanup=True, **{"threading.get_ident": 2},
                 force_close_tcp_sockets=raise_("RuntimeError", "fc")))
case("_close_client_on_timeout",
     self=base_self(_protected_cancel_check=cc(begin=True)),
     io=close_io(begin_timeout_cleanup=True, **{"threading.get_ident": 1},
                 close_quietly=raise_("OSError", "cq")))
case("_close_client_on_timeout",
     self=base_self(_protected_cancel_check=cc(begin=True)),
     io=close_io(begin_timeout_cleanup=True, **{"threading.get_ident": 1},
                 evict_cached_client_instance=raise_("ValueError", "ev")))

# ── check_cancelled (8) ────────────────────────────────────────────────────
case("check_cancelled", self=base_self(total_timeout=None), io=dict(QUIET_IO))
case("check_cancelled", self=base_self(total_timeout=0, _start=0, hard_deadline=50,
                                        _progress_deadline=50),
     io=dict(QUIET_IO, **{"time.monotonic": seq(100, 101, 102),
                         "begin_timeout_cleanup": True, "threading.get_ident": 1}))
case("check_cancelled", self=base_self(total_timeout=0, _start=0, hard_deadline=50,
                                        _progress_deadline=50, timed_out=ev(True)),
     io=dict(QUIET_IO, **{"time.monotonic": seq(100, 101, 102)}))
case("check_cancelled", self=base_self(total_timeout=None),
     io={"aux_interrupt_cancel_requested": True})
case("check_cancelled", self=base_self(total_timeout=None),
     io={"aux_interrupt_cancel_requested": raise_("ValueError", "v")})
case("check_cancelled", self=base_self(total_timeout=None),
     io=dict(QUIET_IO, is_interrupted=True, aux_interrupt_protected=False))
case("check_cancelled", self=base_self(total_timeout=None),
     io=dict(QUIET_IO, is_interrupted=True, aux_interrupt_protected=True))
case("check_cancelled", self=base_self(total_timeout=None),
     io={"aux_interrupt_cancel_requested": raise_("SystemExit", "bye")})

# ── _watchdog_fire (5) ─────────────────────────────────────────────────────
case("_watchdog_fire", self=base_self(hard_deadline=100, _progress_deadline=100),
     io={"time.monotonic": 10, "threading.Timer": {"id": "t1"}, "timer.start": None})
case("_watchdog_fire", self=base_self(hard_deadline=100, _progress_deadline=100,
                                       timed_out=ev(True)),
     io={"time.monotonic": 10})
case("_watchdog_fire", self=base_self(hard_deadline=5, _progress_deadline=5,
                                       _protected_cancel_check=cc(begin=True)),
     io={"time.monotonic": 10, **close_io(begin_timeout_cleanup=True)})
case("_watchdog_fire", self=base_self(), io={"time.monotonic": raise_("ValueError", "t")})
case("_watchdog_fire", self=base_self(hard_deadline=100, _progress_deadline=100),
     io={"time.monotonic": 10, "threading.Timer": raise_("RuntimeError", "timer")})

# ── _arm_timer (5) ─────────────────────────────────────────────────────────
for i, d in enumerate([0, 1.5, 10, 0.25]):
    case("_arm_timer", tag=i, self=base_self(), delay=d,
         io={"threading.Timer": {"id": "t%d" % i}, "timer.start": None})
case("_arm_timer", self=base_self(), delay=3,
     io={"threading.Timer": {"id": "t"}, "timer.start": raise_("ValueError", "s")})

# ── start (5) ──────────────────────────────────────────────────────────────
case("start", self=base_self(total_timeout=None), io=dict(QUIET_IO))
case("start", self=base_self(total_timeout=10, hard_deadline=100, _progress_deadline=100),
     io=dict(QUIET_IO, **{"time.monotonic": 10, "threading.Timer": {"id": "t"},
                          "timer.start": None}))
case("start", self=base_self(total_timeout=10),
     io=dict(QUIET_IO, **{"time.monotonic": raise_("ValueError", "m")}))
case("start", self=base_self(total_timeout=0), io=dict(QUIET_IO))
case("start", self=base_self(total_timeout=10, hard_deadline=100, _progress_deadline=100),
     io=dict(QUIET_IO, **{"time.monotonic": 10, "threading.Timer": {"id": "t"},
                          "timer.start": raise_("RuntimeError", "s")}))

# ── on_event (5) ───────────────────────────────────────────────────────────
case("on_event", self=base_self(total_timeout=None), event={"kind": "content"},
     io=dict(QUIET_IO, codex_event_has_content=True,
             **{"time.monotonic": 5, "notify_aux_provider_response": None}))
case("on_event", self=base_self(total_timeout=None), event={"kind": "keepalive"},
     io=dict(QUIET_IO, codex_event_has_content=False,
             notify_aux_timing_response=None))
case("on_event", self=base_self(total_timeout=None), event={},
     io={"codex_event_has_content": raise_("ValueError", "e")})
case("on_event", self=base_self(total_timeout=None), event={"kind": "content"},
     io=dict(QUIET_IO, codex_event_has_content=True,
             **{"time.monotonic": raise_("RuntimeError", "m")}))
case("on_event", self=base_self(total_timeout=None), event={"kind": "content"},
     io={"codex_event_has_content": True, "time.monotonic": 5,
         "notify_aux_provider_response": None, "aux_interrupt_cancel_requested": True})

# ── _check_deadlines (6) ───────────────────────────────────────────────────
case("_check_deadlines", self=base_self(_total_ceiling=None, _host_deadline=None),
     io={"aux_interrupt_cancel_requested": False, "time.monotonic": 5})
case("_check_deadlines", self=base_self(_total_ceiling=10, _started=0,
                                        _prompt_reading={"seconds": 2}, _host_deadline=None),
     io={"aux_interrupt_cancel_requested": False, "time.monotonic": 20})
case("_check_deadlines", self=base_self(_total_ceiling=None, _started=0,
                                        _host_deadline=3),
     io={"aux_interrupt_cancel_requested": False, "time.monotonic": seq(10, 12)})
case("_check_deadlines", self=base_self(_total_ceiling=None, _host_deadline=None),
     io={"aux_interrupt_cancel_requested": raise_("ValueError", "i")})
case("_check_deadlines", self=base_self(_total_ceiling=None, _started=0,
                                        _host_deadline={"__callable__": "hd"}),
     io={"aux_interrupt_cancel_requested": False, "time.monotonic": seq(10, 12),
         "host_deadline": 5})
case("_check_deadlines", self=base_self(_total_ceiling=None, _started=0,
                                        _host_deadline=100),
     io={"aux_interrupt_cancel_requested": False, "time.monotonic": 10})

# ── _feed_reasoning_details (6) ────────────────────────────────────────────
case("_feed_reasoning_details", self=base_self(reasoning_details=[]),
     delta={"reasoning_details": [{"summary": "s"}]})
case("_feed_reasoning_details", self=base_self(reasoning_details=[]),
     delta={"reasoning_details": [{"foo": "bar"}]})
case("_feed_reasoning_details", self=base_self(reasoning_details=[]),
     delta={"model_extra": {"reasoning_details": [{"thinking": "t"}]}})
case("_feed_reasoning_details", self=base_self(reasoning_details=[]),
     delta={"model_extra": "not-a-dict"})
case("_feed_reasoning_details", self=base_self(reasoning_details=[]),
     delta={"reasoning_details": "not-a-list"})
case("_feed_reasoning_details", self=base_self(reasoning_details=[{"old": 1}]),
     delta={"reasoning_details": []})

# ── _feed_tool_calls (6) ───────────────────────────────────────────────────
case("_feed_tool_calls", self=base_self(tool_calls_acc={}),
     delta={"tool_calls": [{"index": 0, "id": "call_1"}]})
case("_feed_tool_calls", self=base_self(tool_calls_acc={}),
     delta={"tool_calls": [{"index": 1, "function": {"name": "f"}}]})
case("_feed_tool_calls", self=base_self(tool_calls_acc={}),
     delta={"tool_calls": [{"index": 2, "function": {"arguments": "{\"a\":"}}]})
case("_feed_tool_calls", self=base_self(tool_calls_acc={}), delta={"tool_calls": []})
case("_feed_tool_calls", self=base_self(tool_calls_acc={}), delta={})
case("_feed_tool_calls", self=base_self(tool_calls_acc={}),
     delta={"tool_calls": [{"index": 0, "id": "c1", "function": {"name": "f", "arguments": "x"}},
                           {"index": 0, "function": {"arguments": "y"}}]})

# ── __getattr__ (5) ────────────────────────────────────────────────────────
for i, nm in enumerate(["chat", "completions", "create", "base_url", "_leak"]):
    case("__getattr__", tag=i, self={}, name=nm)

# ── remember_temperature_rejection (5) ─────────────────────────────────────
case("remember_temperature_rejection", self=base_self(routes=[]),
     provider="x", base_url="u", rejected_kwargs={'model': "m"},
     io={"route_key": "rk", "bare_model": "bm"})
case("remember_temperature_rejection", self=base_self(routes=[["a", "b"]]),
     provider="y", base_url="v", rejected_kwargs={'model': "m2"},
     io={"route_key": "rk2", "bare_model": "bm2"})
case("remember_temperature_rejection", self=base_self(routes=[]),
     provider="x", base_url="u", rejected_kwargs={'model': "m"},
     io={"route_key": raise_("ValueError", "rk")})
case("remember_temperature_rejection", self=base_self(routes=[]),
     provider="x", base_url="u", rejected_kwargs={'model': "m"},
     io={"route_key": "rk", "bare_model": raise_("RuntimeError", "bm")})
case("remember_temperature_rejection", self=base_self(routes=[]),
     provider="x", base_url="u", rejected_kwargs={'model': None},
     io={"route_key": "rk", "bare_model": "bm"})

# ── _get_provider_chain (5) ────────────────────────────────────────────────
for i in range(5):
    case("_get_provider_chain", tag=i, io={})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    from collections import Counter
    cnt = Counter(c["func"] for c in CASES)
    print(f"wrote {len(CASES)} cases to {OUT}")
    for k in sorted(cnt):
        print(f"  {k}: {cnt[k]}")


if __name__ == "__main__":
    main()
