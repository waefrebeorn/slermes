#!/usr/bin/env python3
"""Oracle for hermes_cli/plugins_loader.py — gate w12_pluginsloa_e4b6ac."""
import json
import sys
import types

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

import hermes_cli.plugins_loader as mod               # noqa: E402


def want_for(c):
    fn = c["func"]
    if fn == "dist_name":
        return mod._dist_name(c["req"])
    if fn == "load_error_text":
        exc = SystemExit(c["code"]) if c["systemexit"] else Exception(c["text"])
        return mod._load_error_text(exc)
    if fn == "platform_name":
        m = types.SimpleNamespace(name=c["name"], path=c["path"])
        return mod.PluginLoaderMixin._platform_name_from_manifest(m)
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
        except Exception as exc:                       # noqa: BLE001
            print("MISMATCH %s: python raised %r" % (fn, exc))
            mism += 1
            continue
        if got != want:
            mism += 1
            print("MISMATCH %s: C=%r PY=%r" % (fn, got, want))
    if total == 0:
        print("ORACLE w12_pluginsloa_e4b6ac: 0 cases (link failure in disguise)")
        return 1
    print("ORACLE w12_pluginsloa_e4b6ac: %d cases, %d mismatches" % (total, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
