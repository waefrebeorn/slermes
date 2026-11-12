#!/usr/bin/env python3
"""Oracle for agent/image_eviction_policy.py::outbound_image_retire_count.

The harness (t_port_image_eviction_policy.c) emits one JSON line per case:
{"case":N,"blocks":[...],"reserved_blocks":N,"sizes":[...]|null,
 "reserved_bytes":N,"many_image_safe":bool,"limit":N|null,
 "budget":N,"batch":N,"floor":N,"out":N}
Defaults are taken from the live module constants.
"""
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/agent/image_eviction_policy.py"


def main() -> int:
    ns = {}
    exec(compile(open(SRC, encoding="utf-8").read(), SRC, "exec"), ns)
    fn = ns["outbound_image_retire_count"]

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
        want = fn(
            c["blocks"],
            reserved_blocks=c["reserved_blocks"],
            carrier_bytes_newest_first=c.get("sizes"),
            reserved_bytes=c.get("reserved_bytes", 0),
            many_image_safe=c.get("many_image_safe", False),
            limit=c.get("limit"),
            budget=c.get("budget", ns["OUTBOUND_IMAGE_BUDGET_BYTES"]),
            batch=c.get("batch", ns["IMAGE_EVICTION_BATCH"]),
            floor=c.get("floor", ns["OUTBOUND_IMAGE_FLOOR"]),
        )
        got = c.get("out")
        if want != got:
            mism += 1
            print(f"MISMATCH {c.get('case')}: blocks={c['blocks']} "
                  f"reserved={c['reserved_blocks']} sizes={c.get('sizes')} "
                  f"C={got} PY={want}")
    if total == 0:
        print("ORACLE image_eviction_policy: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE image_eviction_policy: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
