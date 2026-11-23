#!/usr/bin/env python3
"""Differential oracle for w7_runwatchers_db91ce (gateway/run_watchers.py)."""
import asyncio
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/gateway/run_watchers.py"
spec = importlib.util.spec_from_file_location("w7_runwatchers_db91ce_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w7_runwatchers_db91ce_live"] = live
spec.loader.exec_module(live)


def _sleep_ticks(seconds, running_ticks):
    """Run the live _interruptible_sleep with a patched sleep that flips _running."""
    runner = types.SimpleNamespace(_running=running_ticks > 0)
    counter = [0]

    async def fake_sleep(_sec):
        counter[0] += 1
        if counter[0] >= running_ticks:
            runner._running = False

    real = live.asyncio.sleep
    live.asyncio.sleep = fake_sleep
    try:
        asyncio.run(live._interruptible_sleep(runner, seconds))
    finally:
        live.asyncio.sleep = real
    return counter[0]


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "sleep_ticks":
            py = _sleep_ticks(int(r.get("seconds", 0)), int(r.get("running_ticks", 0)))
        elif func == "prune_due":
            py = bool((float(r.get("now", 0)) - float(r.get("last_ts", 0)))
                      > live._SESSION_STORE_PRUNE_INTERVAL)
        elif func == "stall_timeout":
            # _float_env is imported lazily inside the method. Stub gateway.run
            # so the environment value comes from the fixture, not the process.
            env = r.get("env")

            def fake_float_env(name, default):
                if env is None or env == "":
                    return float(default)
                try:
                    return float(env)
                except (TypeError, ValueError):
                    return float(default)

            run_stub = sys.modules.get("gateway.run")
            if run_stub is None:
                run_stub = types.ModuleType("gateway.run")
                sys.modules["gateway.run"] = run_stub
                pkg = sys.modules.get("gateway")
                if pkg is not None:
                    pkg.run = run_stub
            old = getattr(run_stub, "_float_env", None)
            run_stub._float_env = fake_float_env
            try:
                py = live.GatewaySessionWatchersMixin._session_stall_timeout_seconds(object())
            finally:
                if old is None:
                    del run_stub._float_env
                else:
                    run_stub._float_env = old
        elif func == "drain_releases":
            log = []
            keys = r.get("release_keys") or []
            releases = {k: (lambda k=k: log.append(k)) for k in keys}
            runner = types.SimpleNamespace(_pending_cache_releases=releases)
            live.drain_pending_cache_releases(runner)
            py = log
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} {r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} {r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w7_runwatchers_db91ce: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
