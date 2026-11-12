#!/usr/bin/env python3
"""Oracle for hermes_cli/version_info.py — the pure identity helpers."""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

from hermes_cli.version_info import (          # noqa: E402
    VersionInfo, _derived_version, _parse_nonnegative, hermes_user_agent,
)


def main() -> int:
    total = mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fn = c.get("fn")
        i = c.get("in") or {}
        try:
            if fn == "derived":
                want = _derived_version(i["base"], i.get("distance"),
                                        i.get("dirty", False),
                                        i.get("short_commit"))
            elif fn == "parse_nonnegative":
                want = _parse_nonnegative(i.get("value"))
            elif fn == "display":
                # The property: unknown base falls back to the derived identity.
                info = VersionInfo(i["base"], i["derived"],
                                   i.get("distance"), None, None, "build")
                want = info.display_version
            elif fn == "user_agent":
                # hermes_user_agent reads live provenance; the harness is given
                # the base version directly, so check the same format string.
                want = f"{i['product']}/{i['base_version']}"
            else:
                continue
        except Exception as exc:                       # noqa: BLE001
            print(f"MISMATCH {c.get('case')} ({fn}): python raised {exc!r}")
            mism += 1
            continue
        raw = c["out"]
        got = raw["str"] if isinstance(raw, dict) and set(raw) == {"str"} else raw
        if got != want:
            mism += 1
            print(f"MISMATCH {c.get('case')} ({fn}): C={str(c['out'])[:200]} "
                  f"PY={want}")
    if total == 0:
        print("ORACLE version_info: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE version_info: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
