#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/main_desktop_tcc.py (gate w15_maindeskto_7d5d41).

subprocess.run is injected as an explicit seam: `ok` decides whether run()
raises, `stdout` is the captured stdout passed to the live predicate. The
macOS-only side effects of the CLI never run on this Windows oracle host.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg

SRC = QUARRY + "/hermes_cli/main_desktop_tcc.py"
spec = importlib.util.spec_from_file_location("w15_maindeskto_7d5d41_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w15_maindeskto_7d5d41_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func != "identity_valid":
        return None
    ok = r.get("ok", True)
    stdout = r.get("stdout")

    class _Result:
        pass

    def _run(*a, **k):
        if not ok:
            raise RuntimeError("security find-identity failed")
        res = _Result()
        res.stdout = stdout
        return res

    live.subprocess = types.SimpleNamespace(run=_run)
    return live._macos_codesigning_identity_valid("security", r.get("identity", ""))


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
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w15_maindeskto_7d5d41: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w15_maindeskto_7d5d41: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
