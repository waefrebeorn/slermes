#!/usr/bin/env python3
"""Oracle for gateway/parked_route_refusal.py::_notice_due (pure core).

Harness (t_port_parked_notice.c) emits per case:
  {family:"due", case, sent:<json-array|null>, now:<float>, p, pl, ch,
   out:{"due":bool,"sent":[{profile,platform,chat,ts}...]}}

Live _notice_due is stateful only through the adapter's `_parked_notice_sent_at`
dict and a monotonic clock. Drive the LIVE function with:
  - adapter._parked_notice_sent_at seeded from the emitted sent array (last-write-wins
    on the (profile,platform,chat) key, value = ts)
  - `_monotonic` seeded in its exec-namespace to return the emitted now
then compare due + the surviving sent dict canonically against the C's out.
"""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/gateway/parked_route_refusal.py"
FN = "_notice_due"


def _canon_sent(sent_dict):
    if not sent_dict:
        return []
    return sorted(({"profile": k[0], "platform": k[1], "chat": k[2], "ts": v}
                   for k, v in sent_dict.items()),
                  key=lambda d: (d["profile"], d["platform"], d["chat"]))


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"Any": object, "PARKED_NOTICE_WINDOW_S": 60.0}
    body = [n for n in ast.parse(src).body
            if isinstance(n, ast.FunctionDef) and n.name == FN]
    exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), ns)
    # seed the monotonic clock so the live body's `now` matches the emitted value
    ns["_monotonic"] = lambda: _clock

    class Adapter:
        pass

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        global _clock
        _clock = float(c["now"])
        # seed adapter state from the sent array (last-write-wins)
        sent = {}
        for e in (c.get("sent") or []):
            if isinstance(e, dict):
                sent[(e.get("profile"), e.get("platform"), e.get("chat"))] = float(e.get("ts") or 0.0)
        ad = Adapter()
        ad._parked_notice_sent_at = sent
        live_due = ns[FN](ad, c.get("p"), c.get("pl"), c.get("ch"))
        live_sent = _canon_sent(ad._parked_notice_sent_at)

        got = c.get("out")
        got_due = bool(got.get("due"))
        got_sent = _canon_sent({
            (x.get("profile"), x.get("platform"), x.get("chat")): float(x.get("ts") or 0.0)
            for x in (got.get("sent") or [])})
        if live_due != got_due or live_sent != got_sent:
            mism += 1
            print(f"MISMATCH case={c.get('case')}\n"
                  f"  due C={got_due} PY={live_due}\n"
                  f"  sent C={got_sent}\n"
                  f"  sent PY={live_sent}")
    if total == 0:
        print("ORACLE parked_notice: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE parked_notice: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
