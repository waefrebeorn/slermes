#!/usr/bin/env python3
"""Oracle for hermes_cli/plugin_catalog.py — gate w12_plugincata_a970ad."""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

import hermes_cli.plugin_catalog as mod                 # noqa: E402


def want_for(c):
    fn = c["func"]
    if fn == "is_allowed_image_url":
        return mod.is_allowed_image_url(c["url"])
    if fn == "is_https_url":
        return mod.is_https_url(c["url"])
    if fn == "normalize_repo":
        return mod._normalize_repo(c["url"])
    raise ValueError("unknown func %r" % fn)


def main():
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
        fn = c.get("func")
        got = c.get("out")
        try:
            want = want_for(c)
        except Exception as exc:                        # noqa: BLE001
            print("MISMATCH %s: python raised %r" % (fn, exc))
            mism += 1
            continue
        if got != want:
            mism += 1
            print("MISMATCH %s: C=%r PY=%r url=%r" % (fn, got, want, c.get("url")))
    if total == 0:
        print("ORACLE w12_plugincata_a970ad: 0 cases (link failure in disguise)")
        return 1
    print("ORACLE w12_plugincata_a970ad: %d cases, %d mismatches" % (total, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
