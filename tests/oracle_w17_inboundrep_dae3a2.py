#!/usr/bin/env python3
"""Differential oracle for gateway/inbound_reply_context.py (gate w17_inboundrep_dae3a2).

agent.message_sanitization.strip_internal_markup and gateway.rich_sent_store are
injected as deterministic stand-ins driven by the fixture, so the LIVE pure
helpers run unchanged for every case.
"""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = []
sys.modules["agent"] = agent_pkg
sanit = types.ModuleType("agent.message_sanitization")
SAN = {"v": None}
def _strip(text):
    return SAN["v"] if SAN["v"] is not None else text
sanit.strip_internal_markup = _strip
sys.modules["agent.message_sanitization"] = sanit

gw_pkg = types.ModuleType("gateway")
gw_pkg.__path__ = []
sys.modules["gateway"] = gw_pkg
rss = types.ModuleType("gateway.rich_sent_store")
HEADER = {"v": None}
RM = []
rss.lookup_marker = lambda ck, mid, mk: HEADER["v"]
def _record(ck, ids, mk, hdr):
    RM.append({"chat_key": ck, "ids": list(ids), "marker": mk, "header": hdr})
rss.record_marker = _record
sys.modules["gateway.rich_sent_store"] = rss

SRC = QUARRY + "/gateway/inbound_reply_context.py"
spec = importlib.util.spec_from_file_location("w17_inboundrep_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def platform_of(r):
    p = r.get("platform")
    if isinstance(p, dict) and "value" in p:
        return types.SimpleNamespace(value=p["value"])
    return p


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    try:
        if func == "chat_key":
            py = live._chat_key(platform_of(r), r.get("chat_id"))
        elif func == "field":
            py = live._field(r.get("result"), r.get("key", ""))
        elif func == "pointer":
            SAN["v"] = r.get("sanitized", r.get("text", ""))
            HEADER["v"] = r.get("header")
            replied = ("telegram", "c1", "m1") if r.get("has_message_id") else None
            py = live.reply_pointer(r.get("text", ""),
                                    own_message=bool(r.get("own_message")),
                                    replied_to=replied)
        elif func == "record":
            RM.clear()
            live.record_cron_delivery(platform_of(r), r.get("chat_id"),
                                      r.get("text", ""), r.get("send_result"))
            py = RM[0] if RM else None
        else:
            continue
    except Exception as exc:
        mism += 1
        print("LIVE ERROR case=%s %r" % (r.get("case"), exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH case=%s func=%s\n  py=%r\n  c =%r" %
              (r.get("case"), func, py, got), file=sys.stderr)

print("ORACLE w17_inboundrep_dae3a2: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
