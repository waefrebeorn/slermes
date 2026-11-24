#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_host_terminal.py (w16_webhostte_70d7f1).

request_allowed's imported globals (app.state, _LOOPBACK_HOSTS, policy) are
injected as fake modules from the fixture so no live server is needed.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/hermes_cli/web_host_terminal.py"
spec = importlib.util.spec_from_file_location("w16_webhostte_70d7f1_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def allowed_case(r):
    state = types.SimpleNamespace(
        host_terminal=bool(r.get("host_terminal", False)),
        auth_required=bool(r.get("auth_required", False)),
        bound_host=r.get("bound_host", ""))
    server = types.ModuleType("hermes_cli.web_server")
    server.app = types.SimpleNamespace(state=state)
    chat = types.ModuleType("hermes_cli.web_server_chat")
    chat._LOOPBACK_HOSTS = set(r.get("loopback_hosts", []))
    surface = types.ModuleType("hermes_cli.web_server_surface")
    surface.policy = lambda st: types.SimpleNamespace(host_terminal=st.host_terminal)
    saved = {k: sys.modules.get(k) for k in
             ("hermes_cli.web_server", "hermes_cli.web_server_chat", "hermes_cli.web_server_surface")}
    sys.modules["hermes_cli.web_server"] = server
    sys.modules["hermes_cli.web_server_chat"] = chat
    sys.modules["hermes_cli.web_server_surface"] = surface
    try:
        return live.request_allowed()
    finally:
        for k, v in saved.items():
            if v is None:
                sys.modules.pop(k, None)
            else:
                sys.modules[k] = v


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
    if func not in ("lc", "dim", "allowed"):
        continue
    cases += 1
    try:
        if func == "lc":
            py = live.terminal_lc_ctype(r.get("env", {}), r.get("platform", ""))
        elif func == "dim":
            py = live.query_dimension(
                raw=r.get("raw"), default=int(r.get("default", 10)),
                maximum=int(r.get("maximum", 100)))
        else:
            py = allowed_case(r)
    except Exception as exc:
        py = {"error": repr(exc)}
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print("ORACLE w16_webhostte_70d7f1: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
