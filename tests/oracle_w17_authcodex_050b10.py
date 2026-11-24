#!/usr/bin/env python3
"""Differential oracle for hermes_cli/auth_codex_browser.py (gate w17_authcodex_050b10).

Module-level imports (auth_constants, auth_device_flow) are stubbed so the live
module loads; load_config_readonly is stubbed to return the per-case config,
mirroring the injected `config` argument in the C port.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


hcli = _mod("hermes_cli")
hcli.__path__ = []
_mod("hermes_cli.auth_constants",
     AuthError=type("AuthError", (RuntimeError,), {}),
     CODEX_OAUTH_CLIENT_ID="app_EMoamEEZ73f0CkXaXp7hrann",
     CODEX_OAUTH_TOKEN_URL="https://auth.openai.com/oauth/token",
     _codex_err=lambda msg, code: RuntimeError(msg))
_noop = lambda *a, **k: None
_mod("hermes_cli.auth_device_flow",
     _bind_loopback_callback_server=_noop,
     _can_open_graphical_browser=_noop,
     _make_loopback_callback_handler=_noop,
     _pkce_code_challenge=_noop,
     _pkce_code_verifier=_noop,
     _print_loopback_ssh_hint=_noop,
     _serve_loopback_callback=_noop)
cfg_mod = _mod("hermes_cli.config", load_config_readonly=lambda: None)

SRC = Q + "/hermes_cli/auth_codex_browser.py"
spec = importlib.util.spec_from_file_location("w17_authcodex_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w17_authcodex_live"] = live
spec.loader.exec_module(live)

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
    try:
        if func == "authorize_url":
            py = live._codex_browser_authorize_url(
                redirect_uri=r.get("redirect_uri", ""),
                state=r.get("state", ""),
                code_challenge=r.get("code_challenge", ""))
        elif func == "login_flow":
            cfg_mod.load_config_readonly = lambda c=r.get("config"): c
            args = types.SimpleNamespace(browser=r.get("browser", False))
            py = live._codex_login_flow(args)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w17_authcodex_050b10: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
