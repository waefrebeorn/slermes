#!/usr/bin/env python3
"""Oracle for upstream Hermes cron/quota_hold.py (gate w5_quotahold_3054bf).

hold_seconds_from_failure is driven with a synthetic cause chain of stubbed
AuthError objects; hold_active/complete/is_recovery use stubbed cron.jobs and
hermes_time leaves. No network/filesystem is touched.
"""
import copy
import importlib.util
import json
import sys
import types
from datetime import datetime, timezone

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


_pkg("cron", QUARRY + "/cron")
_pkg("hermes_cli", QUARRY + "/hermes_cli")

# cron.extra_fire is imported at module load; stub its needed surface.
ef = types.ModuleType("cron.extra_fire")
ef.queued = lambda job: False
ef.reschedule = lambda job, at: None
sys.modules["cron.extra_fire"] = ef
sys.modules["cron"].extra_fire = ef

# hermes_time leaves used by _window_end / hold_notice (not invoked here, but
# imported at module load).
ht = types.ModuleType("hermes_time")
ht.now = lambda: datetime.now(timezone.utc)
ht.safe_strftime = lambda d, fmt: d.strftime(fmt)
sys.modules["hermes_time"] = ht

# cron.jobs datetime helpers used by hold_active.
def _parse_aware(value):
    if not isinstance(value, str):
        return None
    try:
        d = datetime.fromisoformat(value)
        if d.tzinfo is None:
            d = d.replace(tzinfo=timezone.utc)
        return d
    except Exception:
        return None


def _instant_after(a, b):
    return a.astimezone(timezone.utc) > b.astimezone(timezone.utc)


jobs = types.ModuleType("cron.jobs")
jobs._parse_aware = _parse_aware
jobs._instant_after = _instant_after
sys.modules["cron.jobs"] = jobs


class FakeAuthErr(Exception):
    def __init__(self, rate_limited, retry_after, text):
        super().__init__(text)
        self.rate_limited = rate_limited
        self.retry_after = retry_after


auth = types.ModuleType("hermes_cli.auth")
auth.AuthError = FakeAuthErr
auth.is_rate_limited_auth_error = lambda e: bool(getattr(e, "rate_limited", False))
sys.modules["hermes_cli.auth"] = auth
sys.modules["hermes_cli"].auth = auth

spec = importlib.util.spec_from_file_location("cron.quota_hold", QUARRY + "/cron/quota_hold.py")
live = importlib.util.module_from_spec(spec)
sys.modules["cron.quota_hold"] = live
spec.loader.exec_module(live)

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
    job = r.get("job") or {}
    if func == "hold_seconds":
        entries = r.get("chain") or []
        chain = [FakeAuthErr(e.get("rate_limited", False), e.get("retry_after"), e.get("text", ""))
                 for e in entries]
        for i in range(1, len(chain)):
            chain[i - 1].__cause__ = chain[i]
        py = live.hold_seconds_from_failure(chain[0] if chain else None)
    elif func == "hold_active":
        py = live.hold_active(job, datetime.fromisoformat(r["now"]))
    elif func == "clear":
        j = copy.deepcopy(job)
        live.clear_state(j)
        py = j
    elif func == "is_recovery":
        py = live.is_recovery_fire(job, r.get("next_run", ""))
    else:
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w5_quotahold_3054bf: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
