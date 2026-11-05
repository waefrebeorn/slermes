#!/usr/bin/env python3
"""Oracle for agent/session_activity.py pure core.

is_terminal / bound / normalize / build_activity_snapshot compare directly
against the live module. format_iteration_progress calls the i18n `t()` which
cannot load locales without ruamel, so its output is checked against the
canonical en.yaml templates ("iteration {n}" / "iteration {n}/{max}") — the
English default the product ships with. The AwakeIdleMeter is driven with a
scripted fake clock so construction and every measure consume identical
(wall, mono) pairs.
"""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import agent.session_activity as SA  # noqa: E402

PROVS = [
    "agent.compression_timeout", "agent.compression_cooldown",
    "agent.compression_turnhold", "agent.compression", "unknown", "",
    "  agent.compression_cooldown  ", "BOGUS", None,
    "agent.compression.turnhold", "AGENT.COMPRESSION_COOLDOWN",
]

DESCS = [
    None, "", "   ", "hello world",
    "a very long activity description that comfortably exceeds the one hundred and twenty character budget that the module reserves for the persistent session viewer column so it must be clamped and ellipsized exactly",
]

ITERS = [
    ("3", "15"), ("0", "15"), ("3", None), ("3", ""), ("3", "9223372036854775807"),
    ("3", "3.5"), ("3", "015"), ("3", "abc"), ("-1", "5"), ("12", "0"),
]

SNAPS = [
    (None, "hello", "agent.compression", 100.0, None),
    ("50.0", "work", "agent.compression_timeout", 100.3, None),
    (None, None, None, 0.0, None),
    ("12.5", "  padded  ", "bogus", 22.2, {"note": "hi", "n": 7, "ok": True}),
    ("9.1", "", "", 19.0, {}),
]

PROGRESS_TEMPLATES = {
    "unbounded": "iteration {n}",
    "bounded": "iteration {n}/{max}",
}

# Scripted (idle, wall, mono): the first pair anchors construction.
METER_SCRIPT = [
    (50.0, 1005.0, 2001.0),
    (30.0, 1010.0, 2010.0),
    (2.0, 1020.0, 2015.0),
    (60.0, 1030.0, 2020.0),
    (5.0, 1040.0, 2021.0),
]


class _FakeClock:
    """time.time() returns wall, time.monotonic() returns mono — one pair each."""

    def __init__(self, pair):
        self._wall, self._mono = pair

    def time(self):
        return self._wall

    def monotonic(self):
        return self._mono


def _py_iter_progress(n_text, max_iterations):
    try:
        cap = int(max_iterations) if max_iterations is not None else None
        if cap is None:
            cap = max_iterations
    except (TypeError, ValueError):
        cap = None
    import sys as _s
    if cap is None or (isinstance(cap, int) and cap >= _s.maxsize):
        return PROGRESS_TEMPLATES["unbounded"].format(n=n_text)
    return PROGRESS_TEMPLATES["bounded"].format(n=n_text, max=cap)


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
        print("ORACLE session_activity: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        fam, case = c["family"], c["case"]
        if fam == "is_terminal":
            want = SA.is_terminal_compression_provenance(PROVS[int(case.split("_")[2])])
            if bool(c["ret"]) != bool(want):
                mism += 1
                print(f"MISMATCH {case}: C={c['ret']} PY={want}")
        elif fam == "normalize":
            want = SA.normalize_activity_provenance(PROVS[int(case.split("_")[1])]).value
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH {case}: C={c['out']!r} PY={want!r}")
        elif fam == "bound":
            want = SA.bound_activity_description(c["desc"])
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH {case}: C={c['out']!r} PY={want!r}")
        elif fam == "iter":
            n, mx = ITERS[int(case.split("_")[1])]
            want = _py_iter_progress(n, mx)
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH {case}: C={c['out']!r} PY={want!r}")
        elif fam == "snapshot":
            at, desc, prov, now, extra = SNAPS[int(case.split("_")[1])]
            want = SA.build_activity_snapshot(
                last_activity_at=float(at) if at is not None else None,
                last_activity_description=desc,
                last_activity_provenance=prov,
                now=now, extra=extra)
            got = c["out"]  # harness emitted the snapshot as a JSON object
            if got != want:
                for k in set(got) | set(want):
                    if got.get(k) != want.get(k):
                        mism += 1
                        print(f"MISMATCH {case}.{k}: C={got.get(k)!r} PY={want.get(k)!r}")
        elif fam == "measure":
            idx = int(case.split("_")[1])
            # Drive the real AwakeIdleMeter with one scripted (wall, mono) pair
            # per measure call; SA.time is rebound so time.time()/monotonic()
            # resolve to the fake clock.
            meter = SA.AwakeIdleMeter.__new__(SA.AwakeIdleMeter)
            meter._wall, meter._mono, meter._asleep_s = 1000.0, 2000.0, 0.0
            want = None
            for k in range(idx + 1):
                idle, wall, mono = METER_SCRIPT[k]
                SA.time = _FakeClock((wall, mono))
                want = meter.measure(idle)
            if abs(c["ret"] - want) > 1e-9:
                mism += 1
                print(f"MISMATCH {case}: C={c['ret']} PY={want}")
        else:
            continue

    print(f"ORACLE session_activity: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
