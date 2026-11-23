#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/bot_loop_guard.py (noc_botloop.h)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/gateway/bot_loop_guard.py"
sys.path.insert(0, QUARRY)

# Bootstrap parent package for relative/late imports (hermes_cli.config.cfg_get).
if "gateway" not in sys.modules:
    _gp = types.ModuleType("gateway")
    _gp.__path__ = [QUARRY + "/gateway"]
    sys.modules["gateway"] = _gp
if "hermes_cli" not in sys.modules:
    _hp = types.ModuleType("hermes_cli")
    _hp.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _hp

spec = importlib.util.spec_from_file_location("botloop_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["botloop_live"] = live
spec.loader.exec_module(live)


class _PlatformConfig:
    pass


def _settings(d):
    return live.BotLoopGuardSettings(**d)


def compute(r):
    func = r.get("func")
    if func == "as_bool":
        return live._as_bool(r.get("raw"), r.get("default", False))
    if func == "as_positive":
        return live._as_positive(r.get("raw"), r.get("default", 0.0))
    if func == "as_positive_int":
        return live._as_positive_int(r.get("raw"), r.get("default", 0))
    if func == "configured_bot_user":
        pc = _PlatformConfig()
        pc.extra = r.get("extra")
        return live.configured_bot_user(pc, r.get("user_id"))
    if func == "settings":
        s = live.settings_from_config({"gateway": {"bot_loop_guard": r.get("block")}})
        return dict(enabled=s.enabled, max_events=s.max_events,
                    window_seconds=s.window_seconds,
                    cooldown_seconds=s.cooldown_seconds,
                    max_hops=s.max_hops,
                    bot_replies_expect_reply=s.bot_replies_expect_reply)
    if func == "guard":
        s = _settings(r["settings"])
        g = live.BotLoopGuard(settings=lambda: s, clock=lambda: 0.0)
        results = []
        for op in r["ops"]:
            t = op.get("t", 0.0)
            g._clock = lambda t=t: t
            w = op["op"]
            if w == "admit":
                results.append(list(g.admit(op["conv"])))
            elif w == "blocked":
                results.append(g.blocked(op["conv"]))
            elif w == "chain":
                results.append(list(g.chain_admit(
                    op["conv"], op["sender"], op.get("text"),
                    is_bot=bool(op.get("is_bot", False)))))
            elif w == "note":
                g.note_reply(op["conv"], op.get("recipient", ""))
                results.append(None)
        return results
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE botloop: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH botloop {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE botloop: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
