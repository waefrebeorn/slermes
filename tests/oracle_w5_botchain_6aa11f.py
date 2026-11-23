#!/usr/bin/env python3
"""Oracle for upstream Hermes agent/bot_chain.py (gate w5_botchain_6aa11f).

_hops and Admission.envelope are compared directly. carry_chain is measured by
the chain it moves. admit_send is driven with resolved state/settings facts and
an identity hash (sha256 of the normalized body is replaced by the normalized
body itself, which is injective over the ask domain) so the C leaf can be
compared on the exact duplicate/reply/hop verdicts.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def _pkg(name, path):
    m = types.ModuleType(name)
    m.__path__ = [path]
    sys.modules[name] = m


_pkg("agent", QUARRY + "/agent")

spec = importlib.util.spec_from_file_location("agent.bot_chain", QUARRY + "/agent/bot_chain.py")
live = importlib.util.module_from_spec(spec)
sys.modules["agent.bot_chain"] = live
spec.loader.exec_module(live)


class _IdentHash:
    def __init__(self, data):
        self._s = data.decode("utf-8", "replace")

    def hexdigest(self):
        return self._s


live.hashlib.sha256 = lambda data: _IdentHash(data)

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
        if func == "hops":
            py = live._hops(r.get("author"))
        elif func == "envelope":
            py = live.Admission(hop=r.get("hop", 0)).envelope()
        elif func == "carry":
            old, new = r.get("old", ""), r.get("new", "")
            live._CHAINS = {}
            if old:
                live._CHAINS[old] = {"marker": 1}
            live.carry_chain(old, new)
            py = bool(old) and (old not in live._CHAINS) and (new in live._CHAINS)
        elif func == "admit":
            f = r["facts"]
            settings = SimpleNamespace(
                enabled=f.get("enabled", True),
                max_hops=f.get("max_hops", 4),
                bot_replies_expect_reply=f.get("bot_replies_expect_reply", False),
                cooldown_seconds=f.get("cooldown_seconds", 600),
            )
            live._settings = lambda: settings
            live.time.monotonic = lambda: 1000.0
            key = f.get("key", "k")
            state = {
                "hops": f.get("hops", 0),
                "sent": f.get("sent", 0),
                "asks": set(tuple(x) for x in f.get("asks", [])),
                "no_reply_from": f.get("no_reply_from"),
                "updated": 1000.0,
            }
            live._CHAINS = {key: state}
            agent = SimpleNamespace(session_id=key, _turn_author=f.get("author"),
                                    _bot_chain_session=key)
            ad = live.admit_send(agent, f.get("recipients", []), f.get("body", ""))
            py = {
                "refusal": ad.refusal,
                "refused": ad.refused,
                "notify_only": sorted(ad.notify_only),
                "hop": ad.hop,
                "asks": sorted([list(x) for x in ad.asks.values()]),
                "prior_sent": ad.prior_sent,
            }
        else:
            continue
    except Exception as exc:
        print("ORACLE botchain: live raised %r on %r" % (exc, r), file=sys.stderr)
        mism += 1
        cases += 1
        continue

    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w5_botchain_6aa11f: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
