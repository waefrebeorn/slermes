#!/usr/bin/env python3
"""Oracle for hermes_cli/plugin_session.py — gate w12_pluginsess_6b33f7."""
import json
import sys
import types

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

import hermes_cli.plugin_session as mod              # noqa: E402
import gateway.session_context as gsc                # noqa: E402
import agent.runtime_cwd as rc                       # noqa: E402


def want_for(c):
    fn = c["func"]
    if fn == "session_surface":
        platform = c["platform"]
        env = c["env_platform"]
        op, oe = mod._active_policy, gsc.get_session_env
        mod._active_policy = lambda: (types.SimpleNamespace(platform=platform)
                                      if platform is not None else None)
        gsc.get_session_env = lambda k, d: env
        try:
            return mod.session_surface()
        finally:
            mod._active_policy, gsc.get_session_env = op, oe
    if fn == "context_surface":
        present = bool(c["cli_ref"])
        surface = c["surface"]
        op = mod.session_surface
        mod.session_surface = lambda: surface
        mgr = types.SimpleNamespace(_cli_ref=(object() if present else None))
        try:
            return mod.context_surface(mgr)
        finally:
            mod.session_surface = op
    if fn == "session_cwd":
        scoped, cwd, tcwd = c["scoped"], c["policy_cwd"], c["terminal_cwd"]
        op, osc, otc = mod._active_policy, rc.scoped_session_cwd, rc.scope_terminal_cwd
        mod._active_policy = lambda: (types.SimpleNamespace(cwd=cwd)
                                      if cwd is not None else None)
        rc.scoped_session_cwd = lambda: scoped
        rc.scope_terminal_cwd = lambda: tcwd
        try:
            return mod.session_cwd()
        finally:
            mod._active_policy, rc.scoped_session_cwd, rc.scope_terminal_cwd = op, osc, otc
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
        except Exception as exc:                      # noqa: BLE001
            print("MISMATCH %s: python raised %r" % (fn, exc))
            mism += 1
            continue
        if got != want:
            mism += 1
            print("MISMATCH %s: C=%r PY=%r" % (fn, got, want))
    if total == 0:
        print("ORACLE w12_pluginsess_6b33f7: 0 cases (link failure in disguise)")
        return 1
    print("ORACLE w12_pluginsess_6b33f7: %d cases, %d mismatches" % (total, mism))
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
