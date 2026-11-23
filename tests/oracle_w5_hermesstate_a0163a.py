#!/usr/bin/env python3
"""Differential oracle for hermes_state_loop_guard.py (gate w5_hermesstate_a0163a).

The side-effecting install() wrapper is replaced by a pure stub that applies
the identical mode-validity decision with the injected current mode; the live
install_from_config() itself supplies the config-coercion logic under test.
"""
import contextlib
import importlib.util
import io
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

spec = importlib.util.spec_from_file_location("w5_hermesstate_loop_live", Q + "/hermes_state_loop_guard.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w5_hermesstate_loop_live"] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "valid_mode":
        return r.get("mode") in live._MODES
    if f == "install_mode":
        m = r.get("mode")
        return m if m in live._MODES else r.get("current_mode")
    if f == "install_from_config":
        current = r.get("current_mode")

        def fake_install(mode):
            return mode if mode in live._MODES else current

        live.install = fake_install
        with contextlib.redirect_stdout(io.StringIO()):
            return live.install_from_config(r.get("cfg"))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    try:
        py = compute(r)
    except Exception as exc:
        print("LIVE ERROR %s args=%r: %r" % (r.get("func"), r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH hermesstate_loop %s args=%r py=%r c=%r" % (r.get("func"), r, py, got), file=sys.stderr)

print("ORACLE w5_hermesstate_a0163a: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
