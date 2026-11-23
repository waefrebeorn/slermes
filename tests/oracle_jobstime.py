#!/usr/bin/env python3
"""Oracle for upstream Hermes cron/jobs_time.py (jobstime gate)."""
import copy, importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/cron/jobs_time.py"
spec = importlib.util.spec_from_file_location("jobstime_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["jobstime_live"] = live
spec.loader.exec_module(live)

# Closed IANA set; mirrors the C port and avoids a host tzdata dependency.
ZONES = {"UTC", "Etc/UTC", "America/New_York", "America/Los_Angeles",
         "America/Chicago", "Europe/London", "Europe/Berlin", "Asia/Hong_Kong",
         "Asia/Tokyo", "Asia/Kolkata", "Australia/Sydney", "US/Eastern"}


class FakeZI:
    def __init__(self, name):
        if name not in ZONES:
            raise live.ZoneInfoNotFoundError(name)
        self.key = name


live.ZoneInfo = FakeZI


def pyvalue(r):
    k = r.get("kind", "null")
    if k == "null":
        return None
    if k == "bool":
        return bool(r.get("value"))
    if k == "int":
        return int(r.get("value", "0"))
    if k == "float":
        return float(r.get("value", "0"))
    return r.get("value", "")


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
    if func == "grace":
        try:
            py = {"ok": live.normalize_grace_minutes(pyvalue(r))}
        except ValueError:
            py = {"raises": "ValueError"}
    elif func == "sec":
        py = {"seconds": live.grace_seconds_override(pyvalue(r))}
    elif func == "tz":
        try:
            py = {"ok": live.normalize_timezone(pyvalue(r))}
        except ValueError:
            py = {"raises": "ValueError"}
    elif func == "zone":
        v = r.get("value")
        job = {"timezone": v} if r.get("mode") == "dict" else v
        z = live.zone_for_job(job)
        py = {"zone": z.key if z is not None else None}
    elif func == "prune":
        d = copy.deepcopy(r.get("record", {}))
        live.prune_inherited_knobs(d)
        py = d
    elif func == "const":
        py = {"min": live.MIN_GRACE_MINUTES, "max": live.MAX_GRACE_MINUTES,
              "tzkey": live.TIMEZONE_KEY, "gkey": live.GRACE_KEY}
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE jobstime: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
