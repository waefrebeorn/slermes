#!/usr/bin/env python3
"""Differential oracle for w18_sessionscm_da8730 / hermes_cli/sessions_cmd_browse.py."""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/hermes_cli/sessions_cmd_browse.py"
sys.path.insert(0, QUARRY)
if "hermes_cli" not in sys.modules:
    pkg = types.ModuleType("hermes_cli")
    pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = pkg

# Stub the two module-level imports; neither is used by the pure row helpers.
tf = types.ModuleType("hermes_cli.timefmt")
tf.relative_time = lambda *a, **k: ""
sys.modules["hermes_cli.timefmt"] = tf
for _name in ("persistence", "persistence.core", "persistence.core.errors"):
    if _name not in sys.modules:
        _m = types.ModuleType(_name); _m.__path__ = []; sys.modules[_name] = _m
sys.modules["persistence.core.errors"].SessionWriteGuarded = type(
    "SessionWriteGuarded", (Exception,), {})

spec = importlib.util.spec_from_file_location("browse_live_w18", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["browse_live_w18"] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    s = r.get("session", {})
    if f == "status_tag":
        return live._session_status_tag(r.get("status"))
    if f == "label":
        return live._label(s)
    if f == "clip":
        return live._clip(r.get("text", ""), int(r.get("n", 0)))
    if f == "msgs_str":
        return live._msgs_str(s)
    if f == "match":
        return live._match(s, r.get("query", ""))
    return None

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None: continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w18_sessionscm_da8730: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w18_sessionscm_da8730 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_sessionscm_da8730: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
