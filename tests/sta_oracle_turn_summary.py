#!/usr/bin/env python3
"""
sta_oracle_turn_summary.py — Oracle for agent/turn_summary.py.

Replays the LIVE collector + formatters over the harness fixtures.
"""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import agent.turn_summary as TS  # noqa: E402


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
        print("ORACLE turn_summary: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        fam = c["family"]
        if fam == "elapsed":
            want = TS.format_elapsed(c["arg"])
        elif fam == "plural":
            want = TS._pluralize(c["n"], c["noun"])
        elif fam == "tokens":
            want = TS.format_token_flow(c["n"])
        else:
            collector = TS.TurnSummaryCollector()
            for step in c["steps"]:
                result = step["result"]
                if result is not None and step["tool"] == "patch":
                    result = json.loads(result)
                collector.record_tool(step["tool"], result=result,
                                      is_error=step["is_error"])
            want = TS.format_turn_summary(c["elapsed"], collector.tally,
                                          max_segments=c["max_segments"])
        got = c["out"]
        if got != want:
            mism += 1
            print(f"MISMATCH {c['case']} [{fam}]:\n  C ={got!r}\n  PY={want!r}")

    print(f"ORACLE turn_summary: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
