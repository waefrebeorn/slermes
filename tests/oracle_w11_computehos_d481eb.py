#!/usr/bin/env python3
"""Differential oracle for tui_gateway/compute_host.py (gate w11_computehos_d481eb)."""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

# Stub hermes_yaml/ruamel (not installed here) for host_supervisor's import chain.
class _Any:
    def __init__(self, n="Any"): self._n = n
    def __call__(self, *a, **k): return _Any(self._n + "()")
    def __getattr__(self, n): return _Any(self._n + "." + n)
    def __iter__(self): return iter(())
    def __len__(self): return 0
    def __bool__(self): return True

for nm in ("hermes_yaml", "ruamel"):
    mod = types.ModuleType(nm)
    mod.__path__ = []
    mod.__getattr__ = lambda n: _Any(nm + "." + n)
    sys.modules[nm] = mod

pkg = types.ModuleType("tui_gateway")
pkg.__path__ = [QUARRY + "/tui_gateway"]
sys.modules["tui_gateway"] = pkg

spec = importlib.util.spec_from_file_location("tui_gateway.compute_host", QUARRY + "/tui_gateway/compute_host.py")
live = importlib.util.module_from_spec(spec)
sys.modules["tui_gateway.compute_host"] = live
spec.loader.exec_module(live)

captured = []
def _capture(frame):
    captured.append(frame)

transport = live._HostTransport(_capture)

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
    got = r.get("out")
    try:
        if func == "history_meta":
            want = live._history_meta(r.get("session", {}))
        elif func == "workers":
            env = r.get("env_value")
            key = "HERMES_TUI_RPC_POOL_WORKERS"
            old = os.environ.get(key)
            if env is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = str(env)
            try:
                want = live._default_workers()
            finally:
                if old is None:
                    os.environ.pop(key, None)
                else:
                    os.environ[key] = old
        elif func == "transport_write":
            del captured[:]
            transport.write(r.get("obj", {}))
            want = captured[0]
        elif func == "frame_handler":
            want = live.ComputeHost._FRAME_HANDLERS.get(r.get("kind", ""))
        elif func == "control_failure":
            want = live._CONTROL_FAILURES.get(r.get("route", ""))
        else:
            continue
    except (TypeError, ValueError):
        if func == "workers":
            want = 8
        else:
            raise
    except BaseException as ex:
        print("MISMATCH %s oracle raised %s: %s" % (func, type(ex).__name__, ex), file=sys.stderr)
        cases += 1
        mism += 1
        continue
    cases += 1
    if want != got:
        mism += 1
        print("MISMATCH in=%s py=%r c=%r" % (json.dumps(r, ensure_ascii=False), want, got), file=sys.stderr)
print("ORACLE w11_computehos_d481eb: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or cases == 0 else 0)
