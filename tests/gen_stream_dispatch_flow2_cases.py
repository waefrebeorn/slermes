#!/usr/bin/env python3
"""Generate tests/fixtures/stream_dispatch_flow2.jsonl — differential cases for
the gateway/stream_dispatch.py FLOW remainder (src/gateway/stream_dispatch_flow2.c).

Deterministic, >=5 cases per ported func (dispatch / dispatch_inner /
dispatch_tool_call): branch boundaries, scripted sequences, fail-closed seams
(missing/empty script -> RuntimeError), real exception kinds (Exception swallowed
by the dispatch wrapper; BaseException passes through), and the documented
event/state domain. Events are tagged "_type" (dataclass name) and carry every
field explicitly so the real frozen dataclasses materialize without default drift.

Run: python tests/gen_stream_dispatch_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "stream_dispatch_flow2.jsonl")
CASES = []


def R(kind, msg=""):
    return {"__raise__": kind, "msg": msg}


def state(**kw):
    d = {"tool_mode": "all", "preview_max_len": 40, "_last_tool": None,
         "sink": None, "_enqueue_tool_line": {"fn": 1}, "_on_long_tool": None,
         "_on_notice": None, "adapter": {"a": 1}}
    d.update(kw)
    return d


def tool_event(tool_name="shell", preview="p", args=None, index=0):
    return {"_type": "ToolCallChunk", "tool_name": tool_name,
            "preview": preview, "args": args if args is not None else {}, "index": index}


def C(func, io=None, st=None, event=None):
    CASES.append({"func": func,
                  "in": {"io": io if io is not None else {},
                         "state": st if st is not None else state(),
                         "event": event if event is not None else tool_event()}})


# ── dispatch_tool_call (the UNCLAIMED method) ──
C("dispatch_tool_call", {"format_tool_event": "chrome line", "_enqueue_tool_line": None})
C("dispatch_tool_call", {"format_tool_event": None})                       # adapter eats (None)
C("dispatch_tool_call", {"format_tool_event": ""})                         # adapter eats ("")
C("dispatch_tool_call", {}, st=state(tool_mode="off"))                     # off -> no seams
C("dispatch_tool_call", {}, st=state(_enqueue_tool_line=None))             # no enqueue -> no seams
C("dispatch_tool_call", {}, st=state(tool_mode="new", _last_tool="shell"))  # new dedup
C("dispatch_tool_call", {"format_tool_event": "L", "_enqueue_tool_line": None},
  st=state(tool_mode="new", _last_tool="other"))                           # new non-dedup
C("dispatch_tool_call", {"format_tool_event": "L", "_enqueue_tool_line": None},
  st=state(tool_mode="new", _last_tool=None))                              # new, last null
C("dispatch_tool_call", {"format_tool_event": "verbose line", "_enqueue_tool_line": None},
  st=state(tool_mode="verbose"))
C("dispatch_tool_call", {"format_tool_event": "cap0", "_enqueue_tool_line": None},
  st=state(preview_max_len=0))
C("dispatch_tool_call", {"format_tool_event": R("ValueError", "bad args")})  # propagate
C("dispatch_tool_call", {"format_tool_event": R("RuntimeError", "boom")})
C("dispatch_tool_call", {"format_tool_event": "L", "_enqueue_tool_line": R("RuntimeError", "sink down")})
C("dispatch_tool_call", {})                                                # fail-closed format (missing)
C("dispatch_tool_call", {"format_tool_event": []})                         # fail-closed format (empty)
C("dispatch_tool_call", {"format_tool_event": "L"})                        # fail-closed enqueue
C("dispatch_tool_call", {"format_tool_event": R("SystemExit", "bye")})     # BaseException passthrough
C("dispatch_tool_call", {"format_tool_event": "L", "_enqueue_tool_line": R("KeyboardInterrupt")})
C("dispatch_tool_call", {"format_tool_event": ["first", "second"], "_enqueue_tool_line": None})
C("dispatch_tool_call", {}, st=state(tool_mode="new", _last_tool=""), event=tool_event(tool_name=""))

# ── dispatch_inner (_dispatch routing) ──
C("dispatch_inner", {"render_message_event": None}, st=state(sink={"s": 1}),
  event={"_type": "MessageChunk", "text": "hi"})
C("dispatch_inner", {}, st=state(sink=None), event={"_type": "MessageChunk", "text": "hi"})
C("dispatch_inner", {"render_message_event": None}, st=state(sink={"s": 1}),
  event={"_type": "MessageStop", "final": False})
C("dispatch_inner", {"render_message_event": None}, st=state(sink={"s": 1}),
  event={"_type": "Commentary", "text": "interim"})
C("dispatch_inner", {"format_tool_event": "L", "_enqueue_tool_line": None})  # ToolCallChunk
C("dispatch_inner", {"_on_long_tool": None}, st=state(_on_long_tool={"fn": 1}),
  event={"_type": "LongToolHint", "tool_name": "shell", "duration": 9.5})
C("dispatch_inner", {}, st=state(_on_long_tool=None),
  event={"_type": "LongToolHint", "tool_name": "shell", "duration": 9.5})
C("dispatch_inner", {"_on_notice": None}, st=state(_on_notice={"fn": 1}),
  event={"_type": "GatewayNotice", "kind": "restart", "text": "restarting", "extra": {}})
C("dispatch_inner", {}, st=state(_on_notice=None),
  event={"_type": "GatewayNotice", "kind": "online", "text": "up", "extra": {"k": "v"}})
C("dispatch_inner", {}, event={"_type": "ToolCallFinished", "tool_name": "shell",
                               "duration": 1.0, "ok": True, "index": 0})
C("dispatch_inner", {}, event={"_type": "Bogus", "x": 1})                  # unknown kind
C("dispatch_inner", {"render_message_event": R("ValueError", "render bad")},
  st=state(sink={"s": 1}), event={"_type": "MessageChunk", "text": "hi"})  # propagate (no swallow)
C("dispatch_inner", {"format_tool_event": R("ValueError", "bad")})         # propagate via tool call

# ── dispatch (exception-swallowing wrapper) ──
C("dispatch", {"render_message_event": None}, st=state(sink={"s": 1}),
  event={"_type": "MessageChunk", "text": "hi"})
C("dispatch", {"format_tool_event": "L", "_enqueue_tool_line": None})
C("dispatch", {"format_tool_event": R("ValueError", "bad"), "logger.debug": None})  # swallow
C("dispatch", {"format_tool_event": R("SystemExit", "bye")})               # passthrough, no logger
C("dispatch", {"format_tool_event": R("KeyboardInterrupt")})
C("dispatch", {"format_tool_event": R("CancelledError")})
C("dispatch", {"format_tool_event": R("GeneratorExit")})
C("dispatch", {"format_tool_event": "L", "_enqueue_tool_line": R("RuntimeError", "sink down"),
               "logger.debug": None})                                     # swallow enqueue raise
C("dispatch", {"render_message_event": R("ValueError", "render bad"), "logger.debug": None},
  st=state(sink={"s": 1}), event={"_type": "MessageChunk", "text": "hi"})
C("dispatch", {"_on_long_tool": R("ValueError", "hint bad"), "logger.debug": None},
  st=state(_on_long_tool={"fn": 1}),
  event={"_type": "LongToolHint", "tool_name": "shell", "duration": 9.5})
C("dispatch", {"_on_notice": R("RuntimeError", "notice bad"), "logger.debug": None},
  st=state(_on_notice={"fn": 1}),
  event={"_type": "GatewayNotice", "kind": "restart", "text": "x", "extra": {}})
C("dispatch", {"logger.debug": None})                                     # fail-closed format -> swallow
C("dispatch", {"format_tool_event": R("ValueError", "bad"),
               "logger.debug": R("RuntimeError", "logger down")})         # logger raise propagates
C("dispatch", {}, event={"_type": "ToolCallFinished", "tool_name": "shell",
                         "duration": 1.0, "ok": True, "index": 0})
C("dispatch", {}, st=state(sink=None), event={"_type": "MessageStop", "final": True})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases -> %s" % (len(CASES), OUT))
    for k in sorted(counts):
        print("  %-20s %d" % (k, counts[k]))
    assert all(v >= 5 for v in counts.values()), "need >=5 cases per func"


if __name__ == "__main__":
    main()