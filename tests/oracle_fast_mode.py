#!/usr/bin/env python3
"""Oracle for agent/fast_mode.py pure tier helpers.

The module imports cleanly; replay it directly. Harness families:
  pst:  {family:"pst", in:<str|null>, out:<str|null>}    -> parse_service_tier
  pest: {family:"pest", in:<str|null>, err:<0|1> | out}  -> parse_exact_service_tier
          (err=1 means ValueError; out otherwise)
  stw:  {family:"stw", in:<str|null>, out:<str|null>}    -> service_tier_word
"""
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

PYROOT = "C:/Users/eman5/hermes-agent"


def main() -> int:
    sys.path.insert(0, PYROOT)
    import agent.fast_mode as fm

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fam = c.get("family")
        if fam == "pst":
            want = fm.parse_service_tier(c.get("in"))
            got = c.get("out")
            if want != got:
                mism += 1
                print(f"MISMATCH pst case={c.get('case')} in={c.get('in')!r} C={got!r} PY={want!r}")
        elif fam == "pest":
            try:
                want = fm.parse_exact_service_tier(c.get("in"))
                want_err = 0
            except ValueError:
                want = None
                want_err = 1
            got_err = int(c.get("err", 0)) if "err" in c else 0
            got = c.get("out")
            if want_err == 1:
                if got_err != 1:
                    mism += 1
                    print(f"MISMATCH pest case={c.get('case')} in={c.get('in')!r} C=no-err PY=ValueError")
            elif got_err == 1 or got != want:
                mism += 1
                print(f"MISMATCH pest case={c.get('case')} in={c.get('in')!r} C={got!r} PY={want!r}")
        elif fam == "stw":
            want = fm.service_tier_word(c.get("in"))
            got = c.get("out")
            if want != got:
                mism += 1
                print(f"MISMATCH stw case={c.get('case')} in={c.get('in')!r} C={got!r} PY={want!r}")
    if total == 0:
        print("ORACLE fast_mode: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE fast_mode: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
