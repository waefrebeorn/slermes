#!/usr/bin/env python3
"""Oracle for hermes_cli/onboarding_run.py — gate w12_onboarding_37e376.

Reads harness JSON lines from stdin; each echoes its input keys so the live
Python function can be reconstructed with injected seam values. Compares the
harness "out" to the live result. Exit 1 on any mismatch.
"""
import json
import sys
import types

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

import hermes_cli.onboarding_run as mod            # noqa: E402
import hermes_cli.config_effective as ce           # noqa: E402
import hermes_cli.install_state as istate          # noqa: E402


def want_for(c):
    fn = c["func"]
    if fn == "explicit_run":
        section = c["section"]
        orig = ce.load_user_config_effective
        orig_root = mod._root_config
        ce.load_user_config_effective = lambda path: {"onboarding": section}
        mod._root_config = lambda: "dummy-root-config.yaml"
        try:
            return mod._explicit_run()
        finally:
            ce.load_user_config_effective = orig
            mod._root_config = orig_root
    if fn == "should_run":
        explicit = c["explicit"]
        af = bool(c["assume_fresh"])
        fresh = bool(c["fresh"])
        oe, op = mod._explicit_run, istate.probe
        mod._explicit_run = lambda: explicit
        istate.probe = lambda: types.SimpleNamespace(fresh=fresh)
        try:
            return mod.should_run(assume_fresh=af)
        finally:
            mod._explicit_run, istate.probe = oe, op
    if fn == "latch":
        explicit = c["explicit"]
        fresh = bool(c["fresh"])
        oe, op = mod._explicit_run, istate.probe
        osr, orr = mod.set_run, istate.reserve_trial
        mod._explicit_run = lambda: explicit
        istate.probe = lambda: types.SimpleNamespace(fresh=fresh)
        mod.set_run = lambda v: None
        istate.reserve_trial = lambda: None
        try:
            return mod.latch_first_install()
        finally:
            mod._explicit_run = oe
            istate.probe = op
            mod.set_run = osr
            istate.reserve_trial = orr
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
        except Exception as exc:                    # noqa: BLE001
            print("MISMATCH %s: python raised %r" % (fn, exc))
            mism += 1
            continue
        if got != want:
            mism += 1
            print("MISMATCH %s: C=%r PY=%r" % (fn, got, want))
    if total == 0:
        print("ORACLE w12_onboarding_37e376: 0 cases (link failure in disguise)")
        return 1
    print("ORACLE w12_onboarding_37e376: %d cases, %d mismatches" % (total, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
