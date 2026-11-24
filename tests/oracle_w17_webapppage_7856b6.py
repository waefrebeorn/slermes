#!/usr/bin/env python3
"""Oracle for gate w17_webapppage_7856b6 (hermes_cli/webapp_page_policy.py)."""
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

live = _load("hermes_cli.webapp_page_policy", "hermes_cli/webapp_page_policy.py")

def _request(surface):
    state = SimpleNamespace()
    if surface is not None:
        state.ui_surface = surface
    return SimpleNamespace(app=SimpleNamespace(state=state))

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
    if func == "script_hashes":
        py = live._ScriptHashes(r.get("html", "") or "").hashes
    elif func == "page_headers":
        surf = r.get("ui_surface")
        surf = surf if isinstance(surf, str) else None
        hdrs = r.get("headers")
        hdrs = dict(hdrs) if isinstance(hdrs, dict) else {}
        py = live.page_headers(_request(surf), r.get("html", "") or "", hdrs)
    elif func == "constants":
        sample = live.page_headers(_request("webapp"), "", {})
        py = {
            "GONE_SURFACE": "webapp",
            "NOSNIFF": sample.get("X-Content-Type-Options"),
            "CSP_HEADER": "Content-Security-Policy",
        }
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w17_webapppage_7856b6: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
