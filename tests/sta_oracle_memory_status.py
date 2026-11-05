#!/usr/bin/env python3
"""Oracle for gateway/memory_status.py.

Direct live comparison. For collect_memory_status, `_read_state_files` is
monkeypatched to return the exact fixture dicts the harness serialised, and
`now` (moment) is the same fixed UTC instant.
"""
import json
import sys
from datetime import datetime, timezone

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import gateway.memory_status as MS  # noqa: E402

NOW_EPOCH = 1793880000.0  # 2026-11-05T12:00:00+00:00
NOW_DT = datetime.fromtimestamp(NOW_EPOCH, tz=timezone.utc)

VALS = ["0", "1", "65536", "1048576", "131072", "-1", "1.5", "null", "true", '"5"']

PRESS = [("null", "null"), ("null", "8000000"), ("0", "8000000"),
         ("50000", "8000000"), ("70000", "8000000"), ("70000", "200000"),
         ("131072", "100000"), ("200000", "4000000"), ("-5", "4000000"),
         ("100000", "null"), ("500", "1000000"), ("100000", "0")]

TSS = ["2026-11-05T12:00:00+00:00", "2026-11-05T11:59:50", "2026-11-05T12:00:00Z",
       "2026-11-05T10:30:15.500000+00:00", "2026-11-05T12:00:00", "2026-11-05",
       "2026-13-05T12:00:00", "bogus", "", "2026-11-05T99:00:00+00:00",
       "1999-01-01T00:00:00+00:00"]


def _load(j):
    return None if j is None else json.loads(j)


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE memory_status: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        fam, case = c["family"], c["case"]
        if fam == "nonneg":
            v = c["in"]  # already decoded: null/bool/number/string
            want_n = MS._nonneg_int(v)
            want_n = -1 if want_n is None else want_n
            if c["ret"] != want_n:
                mism += 1
                print(f"MISMATCH {case}.nonneg: C={c['ret']} PY={want_n} in={c['in']}")
            want_mb = MS._mb(v)
            want_mb = -1 if want_mb is None else want_mb
            if c["mb"] != want_mb:
                mism += 1
                print(f"MISMATCH {case}.mb: C={c['mb']} PY={want_mb} in={c['in']}")
        elif fam == "classify":
            a, t = c["a"], c["t"]  # already decoded
            want = MS.classify_pressure(a, t)
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH {case}: C={c['out']!r} PY={want!r} a={c['a']}")
        elif fam == "parse":
            want = MS._parse_iso(c["in"])
            if want is None:
                want_ep = -1.0
            else:
                d = want.replace(tzinfo=timezone.utc)
                want_ep = d.timestamp()
            if abs(c["epoch"] - want_ep) > 1e-6:
                mism += 1
                print(f"MISMATCH {case}: C={c['epoch']} PY={want_ep} in={c['in']!r}")
        elif fam == "collect":
            idx = int(case.split("_")[1])
            # rebuild fixtures from committed tables keyed by index
            colls = [
                ('{"updated_at":"2026-11-05T11:59:50","mem":{"rss_kib":102400,"mem_total_kib":16777216,"mem_available_kib":50000,"swap_used_kib":2048}}', None),
                ('{"updated_at":"2026-11-05T12:00:00+00:00","mem":{"rss_kib":4096,"mem_total_kib":16777216,"mem_available_kib":8388608,"swap_used_kib":0}}', None),
                ('{"updated_at":"2026-11-05T10:00:00+00:00","mem":{"rss_kib":102400,"mem_total_kib":16777216,"mem_available_kib":0,"swap_used_kib":100}}', None),
                (None, None),
                ('{"updated_at":"2026-11-05T12:00:00+00:00"}', None),
                ("{}", None),
                ('{"updated_at":"2026-11-05T12:00:00Z","mem":{"rss_kib":4096,"mem_total_kib":16777216,"mem_available_kib":8388608,"swap_used_kib":0}}',
                 '{"prior_unclean_exit":true,"prior_suspected_oom":"x","started_at":"2026-11-01T00:00:00"}'),
                (None, '{"prior_unclean_exit":0,"prior_suspected_oom":false,"started_at":7}'),
                (None, "{}"),
                (None, '{"prior_unclean_exit":1,"prior_suspected_oom":0,"started_at":""}'),
            ]
            hb, sn = colls[idx]
            hb = _load(hb)
            sn = _load(sn)

            def fake_read(home):
                return hb, sn

            MS._read_state_files = fake_read
            want = MS.collect_memory_status(now=NOW_DT)
            got = c["out"]
            for k, v in want.items():
                if got.get(k) != v:
                    mism += 1
                    print(f"MISMATCH {case}.{k}: C={got.get(k)!r} PY={v!r}")
        else:
            continue

    print(f"ORACLE memory_status: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
