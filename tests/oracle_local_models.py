#!/usr/bin/env python3
"""Oracle for port_local_models.c -- drives the LIVE
hermes_cli/web_routers/local_models.py defs. The FastAPI / asyncio half is not
imported; the pure defs are ast-extracted with the module constants they read.
"""
import ast
import collections
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\web_routers\local_models.py"
WANTED = ["_human_gb", "_k_label", "_rate_and_eta"]


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {"collections": collections, "Optional": object, "Any": object,
          "dict": dict, "str": str, "int": int, "float": float, "round": round,
          "max": max, "len": len}
    for node in ast.parse(src).body:
        if isinstance(node, ast.Assign):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
            except Exception:
                pass
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


def trim(samples, now, window):
    """local_models.py:_record_rate's window trim, on a deque."""
    while len(samples) > 1 and now - samples[0][0] > window:
        samples.popleft()
    return samples


def main():
    ns = extract()
    cases = 0
    mism = 0

    gbs = {"one": 1073741824.0, "half": 536870912.0, "zero": 0.0,
           "fraction": 1610612736.0, "small": 1.0}
    ks = {"exact": 2048, "under": 2047, "over": 2049, "zero": 0,
          "negative": -1024, "large": 1234567}
    rates = {
        "normal": ([(0.0, 0.0), (1.0, 100.0)], 100, 200),
        "one-sample": ([(0.0, 0.0)], 100, 200),
        "too-short": ([(0.0, 0.0), (0.25, 100.0)], 100, 200),
        "not-moved": ([(0.0, 100.0), (1.0, 100.0)], 100, 200),
        "window": ([(0.0, 0.0), (2.0, 500.0), (4.0, 900.0)], 900, 1000),
        "no-total": ([(0.0, 0.0), (1.0, 100.0)], 100, None),
        "finished": ([(0.0, 0.0), (1.0, 100.0)], 200, 200),
    }
    trims = {
        "keep-all": ([(0.0, 0.0), (1.0, 50.0), (2.0, 100.0)], 2.0),
        "trim-old": ([(0.0, 0.0), (1.0, 50.0), (10.0, 100.0)], 10.0),
        "keep-one": ([(0.0, 0.0)], 100.0),
        "trim-to-one": ([(0.0, 0.0), (8.0, 50.0), (16.0, 100.0)], 16.0),
    }

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        fn = c.get("fn")
        label = c.get("case")
        cases += 1

        try:
            if fn == "_human_gb":
                want = ns[fn](gbs[label])
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _human_gb case={label} C={got!r} PY={want!r}")

            elif fn == "_k_label":
                want = ns[fn](ks[label])
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _k_label case={label} C={got!r} PY={want!r}")

            elif fn == "_rate_and_eta":
                pairs, done, total = rates[label]
                want_rate, want_eta = ns[fn](collections.deque(pairs), done, total)
                got_hr = bool(c.get("have_rate"))
                got_he = bool(c.get("have_eta"))
                got_rate = c.get("rate")
                got_eta = c.get("eta")
                ok = (got_hr == (want_rate is not None) and
                      (not got_hr or abs(got_rate - want_rate) < 1e-6) and
                      got_he == (want_eta is not None) and
                      (not got_he or got_eta == want_eta))
                if not ok:
                    mism += 1
                    print(f"MISMATCH _rate_and_eta case={label} "
                          f"C=(hr={got_hr},rate={got_rate!r},he={got_he},eta={got_eta!r}) "
                          f"PY=(rate={want_rate!r},eta={want_eta!r})")

            elif fn == "_record_rate_window":
                pairs, now = trims[label]
                want = list(trim(collections.deque(pairs), now, ns["_RATE_WINDOW"]))
                got = c.get("out") or []
                # the harness emits a flat [t0, d0, t1, d1, ...] array
                got_pairs = [(got[i], got[i + 1]) for i in range(0, len(got) - 1, 2)]
                if got_pairs != want:
                    mism += 1
                    print(f"MISMATCH _record_rate_window case={label} C={got_pairs!r} PY={want!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE local_models: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
