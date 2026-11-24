#!/usr/bin/env python3
"""Differential oracle for gateway/run_housekeeping_disk.py (w17_w17_runhouseke_16c95d_16c95d)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


gateway = _mod("gateway"); gateway.__path__ = []
class Platform:
    LOCAL = "local"; RELAY = "relay"
    def __init__(self, value): self.value = value
_mod("gateway.config", Platform=Platform)
_state = {"authority": None}
_mod("gateway.session_authorities", active_authority=lambda runner: _state["authority"])

hcli = _mod("hermes_cli"); hcli.__path__ = []
NOTICE_KEY = "sessions.auto_prune"
_mod("hermes_cli.prune_advisor",
     NOTICE_KEY=NOTICE_KEY,
     should_ask=lambda p: _state["should_ask"].get(str(p)),
     advise=lambda p: _state["advice"].get(str(p)),
     snooze=lambda p: _state["snoozed"].append(str(p)))

agent = _mod("agent"); agent.__path__ = []
def _schedule(coro, loop, **kwargs):
    try:
        coro.close()
    except Exception:
        pass
    return _state.get("future")
_mod("agent.async_utils", safe_schedule_threadsafe=_schedule)
_mod("hermes_constants", get_routing_process_hermes_home=lambda: _state["routing_home"])

SRC = Q + "/gateway/run_housekeeping_disk.py"
spec = importlib.util.spec_from_file_location("w17_runhouseke_16c95d_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w17_runhouseke_16c95d_live"] = live
spec.loader.exec_module(live)

# The profile being scheduled is only visible at the _post() call site; a stub
# records it. Return a coroutine-like object with close() for the scheduler stub.
class _DummyCoro:
    def close(self):
        pass

def _post_stub(runner, profile, home, advice):
    _state["scheduled"].append(profile)
    return _DummyCoro()

live._post = _post_stub


class EventStream:
    def publish(self, chat_id, payload, event_type=""):
        _state["published"].append((chat_id, payload, event_type))
        return True


class Sess:
    def __init__(self):
        self.event_stream = EventStream()


class ES_Auth:
    def __init__(self, sess, chat_id):
        self.sessions = {chat_id: sess} if sess is not None else {}


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "clear":
            _state["published"] = []
            sess = Sess() if r.get("has_live", False) else None
            _state["authority"] = ES_Auth(sess, r.get("chat_id", "c")) if r.get("has_authority", False) else None
            source = types.SimpleNamespace(platform=r.get("platform"), chat_id=r.get("chat_id", "c"))
            live.clear_local_auto_prune_notice(object(), source)
            if not _state["published"]:
                py = None
            else:
                cid, payload, et = _state["published"][0]
                py = {"chat_id": cid, "key": payload.get("key"), "event_type": et}
        elif func == "maybe":
            _state["routing_home"] = r.get("routing_home", "")
            _state["should_ask"] = r.get("should_ask", {})
            _state["advice"] = r.get("advice", {})
            _state["snoozed"] = []
            _state["scheduled"] = []
            fresult = r.get("future_result", False)
            _state["future"] = (None if r.get("future_none", False)
                                else types.SimpleNamespace(result=lambda timeout=None: fresult))
            runner = None if r.get("runner_none", False) else types.SimpleNamespace(
                _served_home_channel_transports=lambda: [tuple(x) for x in r.get("transports", [])],
                _served_profile_homes=r.get("served_profile_homes", {}))
            loop = None if r.get("loop_none", False) else object()
            live.maybe_post_auto_prune_prompt(runner, loop)
            py = {"scheduled": _state["scheduled"], "snoozed": _state["snoozed"]}
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w17_runhouseke_16c95d: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
