#!/usr/bin/env python3
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)
try:
    import ruamel.yaml  # noqa: F401
except Exception:
    _ru = types.ModuleType("ruamel"); _ry = types.ModuleType("ruamel.yaml")
    class _YAMLError(Exception): pass
    class _VersionedResolver: pass
    class _YAML:
        def __init__(self, *a, **k): pass
    _ry.YAML = _YAML; _ry.YAMLError = _YAMLError
    _re = types.ModuleType("ruamel.yaml.error"); _re.YAMLError = _YAMLError
    _rs = types.ModuleType("ruamel.yaml.resolver"); _rs.VersionedResolver = _VersionedResolver
    for _k, _v in (("ruamel", _ru), ("ruamel.yaml", _ry),
                   ("ruamel.yaml.error", _re), ("ruamel.yaml.resolver", _rs)):
        sys.modules[_k] = _v

"""Differential oracle for gateway/session_sudo_prompt.py:ask_sudo."""
SRC = QUARRY + "/gateway/session_sudo_prompt.py"
spec = importlib.util.spec_from_file_location("w14_sessionsud_e00b7b_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

# Fixture-injected seam: capture the request ask_sudo builds and hand it back.
_gw = sys.modules.get("gateway")
if _gw is None:
    _gw = types.ModuleType("gateway"); _gw.__path__ = []; sys.modules["gateway"] = _gw
_ssr = types.ModuleType("gateway.session_server_requests")
def _ask(events, session_id, method, params, *, cancelled=lambda: False, timeout=None, **kw):
    return {"session_id": session_id, "method": method, "params": params, "timeout": timeout}
_ssr.ask_viewer = _ask
sys.modules["gateway.session_server_requests"] = _ssr
_gw.session_server_requests = _ssr

def compute(r):
    func = r.get("func")
    if func == "ask_sudo_request":
        sid, cmd, t = r.get("session_id"), r.get("command"), r.get("timeout")
        if t is None:
            return live.ask_sudo(None, sid, cmd)
        return live.ask_sudo(None, sid, cmd, timeout=t)
    if func == "sudo_timeout_s":
        return live.SUDO_TIMEOUT_S
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
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w14_sessionsud_e00b7b: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
