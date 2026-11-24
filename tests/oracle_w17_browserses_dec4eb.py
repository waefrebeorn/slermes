#!/usr/bin/env python3
"""Oracle for gate w17_browserses_dec4eb (tools/browser_session_sandbox.py)."""
import importlib.util
import json
import sys
import types
import contextlib
import io
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _load(dotted, rel):
    parts = dotted.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pkg = types.ModuleType(pn)
            pkg.__path__ = [QUARRY + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pkg
    spec = importlib.util.spec_from_file_location(dotted, QUARRY + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    sys.modules[dotted] = m
    with contextlib.redirect_stderr(io.StringIO()):
        spec.loader.exec_module(m)
    return m

live = _load("tools.browser_session_sandbox", "tools/browser_session_sandbox.py")

# Drive the live devtools parsers with a controlled streams.run_in.
STATE = {"rc": 0, "out": b"", "exc": False}
fake_streams = types.ModuleType("tools.environments.streams")
def _run_in(*a, **k):
    if STATE["exc"]:
        raise RuntimeError("boom")
    return SimpleNamespace(stdout=STATE["out"], returncode=STATE["rc"], stderr=b"")
fake_streams.run_in = _run_in
fake_envs = types.ModuleType("tools.environments")
fake_envs.streams = fake_streams
sys.modules["tools.environments"] = fake_envs

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
    if func == "shared_key":
        udd = r.get("udd", "")
        raw = live._shared_key(object(), udd)
        # Normalize the nondeterministic id(env) to the fixture's env_id.
        if not (raw.startswith("sandbox:") and raw.endswith(":" + udd)):
            raise AssertionError(f"unexpected live key: {raw!r}")
        env_id = r.get("env_id", "")
        py = "sandbox:" + env_id + ":" + udd
    elif func in ("read_devtools", "live_devtools"):
        STATE["rc"] = int(r.get("rc", 0))
        STATE["out"] = (r.get("text", "") or "").encode("utf-8", "replace")
        STATE["exc"] = bool(r.get("raise_exec", False)) and func == "live_devtools"
        try:
            if func == "read_devtools":
                found = live._read_devtools(object(), "u", "/udd")
            else:
                found = live._live_devtools(object(), "u", "/udd")
        except Exception:
            found = None
        if found is None:
            py = None
        elif isinstance(found, tuple):
            py = list(found)
        else:
            py = found
    elif func == "constants":
        py = {
            "GONE": live.GONE,
            "_GONE_RC": live._GONE_RC,
            "_NOT_OURS_RC": live._NOT_OURS_RC,
            "_CONFIRMED_TTL_S": live._CONFIRMED_TTL_S,
            "_LAUNCH_TIMEOUT_S": live._LAUNCH_TIMEOUT_S,
            "_START_TIME_OF_P": live._START_TIME_OF_P,
            "_STOP_SCRIPT": live._STOP_SCRIPT,
        }
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w17_browserses_dec4eb: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
