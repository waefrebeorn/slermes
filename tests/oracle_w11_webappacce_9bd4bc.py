#!/usr/bin/env python3
"""Differential oracle for gate w11_webappacce_9bd4bc (hermes_cli/webapp_access.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
for _pkg in ("hermes_cli", "gateway"):
    if _pkg not in sys.modules:
        _m = types.ModuleType(_pkg)
        _m.__path__ = [os.path.join(Q, _pkg)]
        sys.modules[_pkg] = _m
NAME = "w11_webappacce_9bd4bc_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "hermes_cli", "webapp_access.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "sign_in_client":
        record = r.get("record", {}) or {}
        live._registration = lambda home: record
        res = live.sign_in_client(r.get("home", "/h"))
        return list(res) if res is not None else None
    if f == "mode":
        settings = r.get("settings", {}) or {}
        wildcard = bool(r.get("wildcard", False))
        gl = types.ModuleType("gateway.webapp_listener")
        gl._is_wildcard = lambda h: wildcard
        sys.modules["gateway.webapp_listener"] = gl
        sys.modules["gateway"].webapp_listener = gl
        return live._mode(types.SimpleNamespace(**settings))
    if f == "serves_config":
        return live.serves_config(r.get("status", {}))
    if f == "write_config":
        cfg = r.get("cfg", {}) or {}
        enabled = bool(r.get("enabled", False))
        host = r.get("host", "")
        public_url = r.get("public_url", "")
        port = r.get("port", None)
        captured = {}
        hc = types.ModuleType("hermes_cli.config")
        hc.load_config = lambda: json.loads(json.dumps(cfg))
        hc.save_config = lambda c: captured.__setitem__("cfg", c)
        sys.modules["hermes_cli.config"] = hc
        sys.modules["hermes_cli"].config = hc
        live._write_config(enabled=enabled, host=host, public_url=public_url, port=port)
        return captured.get("cfg")
    return None


n = m = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if not isinstance(r, dict) or "out" not in r:
        continue
    n += 1
    py, got = compute(r), r.get("out")
    if py != got:
        m += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w11_webappacce_9bd4bc: %d cases, %d mismatches" % (n, m))
sys.exit(1 if m else 0)
