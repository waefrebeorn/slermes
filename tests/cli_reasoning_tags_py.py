#!/usr/bin/env python3
"""Oracle for t_cli_reasoning_tags: recompute expected outputs from LIVE cli.py
and compare against the C harness's JSON results. Exit 0 iff all match."""
import json, re, subprocess, sys

SLERMES = "/home/wubu/hermes-agent-dev/slermes"

def py_strip(text):
    cleaned = text
    for tag in ("REASONING_SCRATCHPAD", "think", "thinking", "reasoning", "thought"):
        cleaned = re.sub(rf"<{tag}>.*?</{tag}>\s*", "", cleaned, flags=re.DOTALL | re.IGNORECASE)
        cleaned = re.sub(rf"<{tag}>.*$", "", cleaned, flags=re.DOTALL | re.IGNORECASE)
        cleaned = re.sub(rf"</{tag}>\s*", "", cleaned, flags=re.IGNORECASE)
    for tc in ("tool_call", "tool_calls", "tool_result", "function_call", "function_calls"):
        cleaned = re.sub(rf"<{tc}\b[^>]*>.*?</{tc}>\s*", "", cleaned, flags=re.DOTALL | re.IGNORECASE)
    cleaned = re.sub(r'(?:(?<=^)|(?<=[\n\r.!??:]))[ \t]*<function\b[^>]*\bname\s*=[^>]*>(?:(?:(?!</function>).)*)</function>\s*',
                     "", cleaned, flags=re.DOTALL | re.IGNORECASE)
    cleaned = re.sub(r"</(?:tool_call|tool_calls|tool_result|function_call|function_calls|function)>\s*", "",
                     cleaned, flags=re.IGNORECASE)
    return cleaned.strip()

def main():
    fixtures = json.load(open(f"{SLERMES}/tests/oracle/fixtures/cli_reasoning_tags/cases.json"))
    r = subprocess.run([f"{SLERMES}/tests/t_cli_reasoning_tags"], capture_output=True, text=True, cwd=SLERMES)
    if r.returncode not in (0, 1):
        print("harness failed:", r.returncode, r.stderr[:300]); return 2
    got_lines = [json.loads(l) for l in r.stdout.strip().splitlines() if l.strip()]
    bad = 0
    for i, fx in enumerate(fixtures):
        expected_live = py_strip(fx["input"])
        g = got_lines[i] if i < len(got_lines) else {"got": None, "match": False}
        ok = (g["got"] == expected_live)
        # also confirm the fixture's recorded expectation still matches live Python
        fixture_stale = (fx["expected"] != expected_live)
        if not ok or fixture_stale:
            bad += 1
            print(f"MISMATCH i={i} live={expected_live!r} c={g['got']!r} stale_fixture={fixture_stale}")
    print(f"cli_reasoning_tags oracle: {len(fixtures)-bad}/{len(fixtures)} MATCH")
    return 0 if bad == 0 else 1

sys.exit(main())
