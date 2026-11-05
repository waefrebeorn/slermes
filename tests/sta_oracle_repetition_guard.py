#!/usr/bin/env python3
"""
sta_oracle_repetition_guard.py — Oracle for agent/repetition_guard.py.

Reads the fixture text the harness emitted and recomputes EVERY check against
the live module, so a mismatch is always a true behavioural difference rather
than a stale expectation table.
"""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import agent.repetition_guard as RG  # noqa: E402


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
        print("ORACLE repetition_guard: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        fam, case = c["family"], c["case"]
        if fam == "dom":
            text = c["text"]
            n = len(text)
            want = {
                "line": RG._line_repetition_dominated(text, n),
                "window": RG._window_count_dominated(text, n),
                "periodic": RG._periodic_run_dominated(text, n),
                "dominated": RG.is_repetition_dominated(text),
                "runaway": RG.is_runaway_repetition(text),
            }
            for k in want:
                if bool(c[k]) != bool(want[k]):
                    mism += 1
                    print(f"MISMATCH {case}.{k}: C={c[k]} PY={want[k]} n={n}")
        elif fam == "expand":
            if "text" in c:
                want = RG._expand_run(c["text"], c["n"], c["start"], c["period"],
                                      c["matched"])
            else:
                text = "abcabcabcabcabc"
                want = RG._expand_run(text, c["n"], c["start"], c["period"],
                                      c["matched"])
            if (c["left"], c["right"]) != want:
                mism += 1
                print(f"MISMATCH {case}: C=({c['left']},{c['right']}) PY={want}")

    print(f"ORACLE repetition_guard: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
