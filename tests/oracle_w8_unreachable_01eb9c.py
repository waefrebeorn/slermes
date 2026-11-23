#!/usr/bin/env python3
"""Differential oracle for w8_unreachable_01eb9c -> cron/unreachable_retry.py.

The four pure predicates are exercised directly.  cron.constants.is_recurring
and hermes_time.now are scripted modules; for _ladder_applies the module-level
retry_enabled() is bound to the fixture's cfg so no real profile config is read.
"""
import importlib.util
import json
import sys
import types

G = "w8_unreachable_01eb9c"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_pkg("cron")
recurring_holder = [False]
_mod("cron.constants", is_recurring=lambda job: recurring_holder[0])
_mod("hermes_time", now=lambda: None)

SRC = Q + "/cron/unreachable_retry.py"
spec = importlib.util.spec_from_file_location("w8_unreachable_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w8_unreachable_live"] = live
spec.loader.exec_module(live)

real_retry_enabled = live.retry_enabled
cfg_holder = [None]
live.retry_enabled = lambda cfg=None: real_retry_enabled(cfg if cfg is not None else cfg_holder[0])


def canonical(x):
    if x is None or isinstance(x, (bool, str, int)):
        return x
    if isinstance(x, float):
        return int(x) if x.is_integer() else x
    if isinstance(x, dict):
        return {k: canonical(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [canonical(v) for v in x]
    return x


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
        if func == "retry_enabled":
            py = real_retry_enabled(r.get("cfg"))
        elif func == "is_retry_fire":
            py = live.is_retry_fire(r.get("job") or {}, r.get("next_run"))
        elif func == "clear_state":
            job = json.loads(json.dumps(r.get("job") or {}))
            live.clear_state(job)
            py = job
        elif func == "ladder_applies":
            recurring_holder[0] = bool(r.get("recurring", False))
            cfg_holder[0] = r.get("cfg")
            py = live._ladder_applies(r.get("job") or {})
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} case={r.get('case')}: {exc!r}", file=sys.stderr)
        mism += 1
        continue
    got = canonical(r.get("out"))
    cases += 1
    if canonical(py) != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={canonical(py)!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or not cases else 0)
