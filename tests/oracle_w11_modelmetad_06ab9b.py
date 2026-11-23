#!/usr/bin/env python3
"""Differential oracle for agent/model_metadata_http.py (gate w11_modelmetad_06ab9b).

Config/TLS/HTTP collaborators are injected from each fixture record via stubs
installed in sys.modules (PORT-KIT: no process env mutation).
"""
import importlib.util, json, sys, types
try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w11_modelmetad_06ab9b"
sys.path.insert(0, QUARRY)
SRC = QUARRY + "/agent/model_metadata_http.py"
_spec = importlib.util.spec_from_file_location("model_metadata_http_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["model_metadata_http_live"] = live
_spec.loader.exec_module(live)

import hermes_cli.urllib_security as _usec  # real url_origin, captured before stubbing

class _Stub(types.ModuleType):
    def __getattr__(self, name):
        if name.startswith("__"): raise AttributeError(name)
        return lambda *a, **k: None
def _seed(name, **attrs):
    m = _Stub(name); m.__path__ = []
    for k, v in attrs.items(): setattr(m, k, v)
    sys.modules[name] = m
    parts = name.split(".")
    for i in range(1, len(parts)):
        p = ".".join(parts[:i])
        if p in sys.modules and not hasattr(sys.modules[p], parts[i]):
            setattr(sys.modules[p], parts[i], m)
    return m

_cfg = _seed("hermes_cli.config", get_custom_provider_tls_settings=lambda url: {})
_sslv = _seed("agent.ssl_verify", resolve_httpx_verify=lambda **kw: {
    "ca_bundle": kw.get("ca_bundle"), "ssl_verify": kw.get("ssl_verify"),
    "base_url": kw.get("base_url")})
_httpx = _seed("httpx", Client=None, Timeout=lambda *a, **k: None)
_dl = _seed("agent.deadline", DeadlineExpired=Exception,
            run_bounded_sync=lambda fn, *a, **k: types.SimpleNamespace(timed_out=False, value=fn()))
_use = _seed("hermes_cli.urllib_security", url_origin=_usec.url_origin)

class _Resp:
    is_success = True
    def read(self): pass
    def close(self): pass
class _Client:
    def __init__(self, **kw): self.kw = kw; _hooks.append(kw)
    def build_request(self, method, url, headers=None, params=None):
        return types.SimpleNamespace(url=url, headers=dict(headers or {}))
    def send(self, req, stream=False): return _Resp()
    def __enter__(self): return self
    def __exit__(self, *a): return False
_hooks = []

def live_out(rec):
    func = rec.get("func")
    if func == "resolve_verify":
        settings = rec.get("settings") or {}
        raise_flag = bool(rec.get("settings_raise"))
        def get_settings(url):
            if raise_flag: raise RuntimeError("config discovery failed")
            return settings
        _cfg.get_custom_provider_tls_settings = get_settings
        return live.resolve_verify(rec.get("base_url") or "")
    if func == "total_deadline_s":
        return live.TOTAL_DEADLINE_S
    if func == "scope_headers":
        headers = rec.get("headers") or {}
        initial = rec.get("initial_url") or ""
        request = rec.get("request_url") or ""
        _httpx.Client = _Client
        del _hooks[:]
        with live.stream(initial, headers=headers, verify="INJECTED"):
            pass
        scope_credentials = _hooks[-1]["event_hooks"]["request"][0]
        req = types.SimpleNamespace(url=request, headers=dict(headers))
        scope_credentials(req)
        return req.headers
    return None

def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        try: rec = json.loads(line)
        except ValueError: continue
        if rec.get("func") not in ("resolve_verify","total_deadline_s","scope_headers"): continue
        cases += 1
        try: py = live_out(rec)
        except Exception as exc: py = {"__raise__": type(exc).__name__, "msg": str(exc)}
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} func={rec.get('func')} C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)"); return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0

if __name__ == "__main__":
    sys.exit(main())
