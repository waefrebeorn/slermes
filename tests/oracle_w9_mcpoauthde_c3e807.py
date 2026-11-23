#!/usr/bin/env python3
"""Differential oracle for tools/mcp_oauth_device.py (gate w9_mcpoauthde_c3e807)."""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m

mcp = _mod("mcp"); mcp.__path__ = [Q + "/mcp"]
shared = _mod("mcp.shared"); shared.__path__ = [Q + "/mcp/shared"]
_mod("mcp.shared.auth", OAuthMetadata=object, OAuthClientInformationFull=object)
_mod("pydantic", AnyHttpUrl=str)

NAME = "w9_mcpoauthde_c3e807_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/tools/mcp_oauth_device.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

class Resp:
    def __init__(self, status, data, bad):
        self.status_code = status
        self._data = data
        self._bad = bad
    def json(self):
        if self._bad:
            raise ValueError("bad json")
        return self._data

def compute(r):
    f = r.get("func")
    if f == "positive_seconds":
        try:
            return {"ok": True, "value": live._positive_seconds(r.get("value"), r.get("label", "interval"))}
        except RuntimeError as e:
            return {"ok": False, "error": str(e)}
    if f == "payload":
        resp = Resp(r.get("status", 200), r.get("data"), bool(r.get("bad_json")))
        try:
            return {"ok": True, "value": live._payload(resp, r.get("label", "payload"))}
        except RuntimeError as e:
            return {"ok": False, "error": str(e)}
    if f == "constants":
        return {"device_grant": live.DEVICE_GRANT}
    if f == "device_grant_supported":
        grants = r.get("grants")
        return not (grants is not None and live.DEVICE_GRANT not in grants)
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
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w9_mcpoauthde_c3e807: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w9_mcpoauthde_c3e807 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w9_mcpoauthde_c3e807: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
