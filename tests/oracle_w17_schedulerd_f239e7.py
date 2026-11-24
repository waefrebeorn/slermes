#!/usr/bin/env python3
"""Differential oracle for cron/scheduler_delivery_media.py (gate w17_schedulerd_f239e7).

Every collaborator of _send_media_via_adapter is injected as a deterministic
stand-in driven by the fixture's `media` entries; the LIVE function body is what
actually runs.  Its returned errors, the recorded adapter send_* calls and the
job's _media_ack_lost latch are compared against the C port's output object.
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

cron_pkg = types.ModuleType("cron")
cron_pkg.__path__ = []
sys.modules["cron"] = cron_pkg

sd = types.ModuleType("cron.scheduler_delivery")
def _note_target_error(job_ref, msg, errors):
    errors.append(msg)
sd._note_target_error = _note_target_error
sys.modules["cron.scheduler_delivery"] = sd

ss = types.ModuleType("cron.scheduler_script")
ss._media_send_timeout_for = lambda p: 30.0
sys.modules["cron.scheduler_script"] = ss

gp = types.ModuleType("gateway.platforms")
gp.__path__ = []
sys.modules["gateway.platforms"] = gp

base = types.ModuleType("gateway.platforms.base")
KEPT = set()
VALID = {}
def _filter(mf):
    return [(p, bool(v)) for p, v in (mf or []) if p in KEPT]
def _validate(path):
    return VALID.get(path, path)
base.BasePlatformAdapter = SimpleNamespace(
    filter_media_delivery_paths=staticmethod(_filter),
    validate_media_delivery_path=staticmethod(_validate))
AUDIO = []
def _audio(platform, ext, is_voice=False):
    return AUDIO.pop(0) if AUDIO else False
base.should_send_media_as_audio = _audio
base.validate_media_delivery_path = _validate
sys.modules["gateway.platforms.base"] = base

agent_pkg = types.ModuleType("agent")
agent_pkg.__path__ = []
sys.modules["agent"] = agent_pkg
au = types.ModuleType("agent.async_utils")
SCEN = {}
CALLS = []
class _Future:
    def __init__(self, sc, path):
        self.sc, self.path = sc, path
    def result(self, timeout=None):
        if self.sc == "timeout":
            raise TimeoutError()
        if self.sc == "ambiguous":
            return SimpleNamespace(success=True, raw_response={"ambiguous": True})
        if self.sc == "failed":
            return SimpleNamespace(success=False, raw_response={}, error="boom")
        return SimpleNamespace(success=True, raw_response={})
    def cancel(self):
        pass
def _sst(coro, loop):
    name, kwargs = coro
    path = [v for k, v in kwargs.items() if k.endswith("_path")][0]
    sc = SCEN.get(path, "ok")
    if sc == "no_future":
        return None
    return _Future(sc, path)
au.safe_schedule_threadsafe = _sst
sys.modules["agent.async_utils"] = au

SRC = QUARRY + "/cron/scheduler_delivery_media.py"
spec = importlib.util.spec_from_file_location("w17_schedulerd_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def make_adapter():
    a = SimpleNamespace(platform="telegram")
    def mk(name):
        def f(**kwargs):
            pk = [k for k in kwargs if k.endswith("_path")][0]
            CALLS.append({"method": name, "path_kw": pk, "path": kwargs[pk]})
            return (name, kwargs)  # sentinel coroutine (never awaited)
        return f
    for nm in ("send_voice", "send_video", "send_image_file", "send_document"):
        setattr(a, nm, mk(nm))
    return a


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") != "send":
        continue
    media = r.get("media") or []
    KEPT.clear(); VALID.clear(); SCEN.clear(); CALLS.clear(); AUDIO.clear()
    for m in media:
        p = m.get("path", "")
        if m.get("kept"):
            KEPT.add(p)
        VALID[p] = m.get("validate", p)
        SCEN[p] = m.get("result", "ok")
        if m.get("kept"):
            AUDIO.append(bool(m.get("audio")))
    job = {}
    try:
        errs = live._send_media_via_adapter(
            make_adapter(), r.get("chat_id", ""),
            [(m.get("path", ""), bool(m.get("voice"))) for m in media],
            None, None, job, platform=r.get("platform"),
            force_document=bool(r.get("force_document")))
    except Exception as exc:
        mism += 1
        print("LIVE ERROR case=%s %r" % (r.get("case"), exc), file=sys.stderr)
        continue
    py = {"errors": errs, "calls": list(CALLS),
          "ack_lost": bool(job.get("_media_ack_lost"))}
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH case=%s\n  py=%r\n  c =%r" % (r.get("case"), py, got), file=sys.stderr)

print("ORACLE w17_schedulerd_f239e7: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
