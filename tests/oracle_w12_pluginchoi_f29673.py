#!/usr/bin/env python3
"""Oracle for hermes_cli/plugin_choices.py — gate w12_pluginchoi_f29673."""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

import hermes_cli.plugin_choices as mod                  # noqa: E402


def want_for(c):
    fn = c["func"]
    if fn == "options":
        return [list(t) for t in mod._options(c["choices"])]
    if fn == "ask_choice":
        tok = mod._ASKER.set(lambda q, l: c["picked"])
        try:
            return mod.ask_choice(c.get("question", "Q?"), c["choices"])
        finally:
            mod._ASKER.reset(tok)
    if fn == "clarify_ask":
        reply = c["reply"]
        ask = mod.clarify_asker(lambda q: reply)
        return ask("q", [])
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
            print("MISMATCH %s: C=%r PY=%r" % (fn, got, want))
    if total == 0:
        print("ORACLE w12_pluginchoi_f29673: 0 cases (link failure in disguise)")
        return 1
    print("ORACLE w12_pluginchoi_f29673: %d cases, %d mismatches" % (total, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
