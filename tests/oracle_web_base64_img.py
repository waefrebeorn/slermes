#!/usr/bin/env python3
"""Oracle for tools/web_tools_truncate.py:convert_base64_images_to_links
(port src/tools/web_base64_img.c). The live module imports cleanly (re + typing
only); replay it directly.

Harness emits {fn:"web_base64_img_convert", in:<str>, out:<str>} for each case."""
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def main() -> int:
    sys.path.insert(0, "C:/Users/eman5/hermes-agent")
    from tools.web_tools_truncate import convert_base64_images_to_links as fn

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            import json
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        want = fn(c.get("in") or "")
        got = c.get("out") or ""
        if want != got:
            mism += 1
            print(f"MISMATCH in={c.get('in')!r} C={got!r} PY={want!r}")
    if total == 0:
        print("ORACLE web_base64_img: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE web_base64_img: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
