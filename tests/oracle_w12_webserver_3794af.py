#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_server_mcp.py (gate w12_webserver_3794af).

Heavy modules are stubbed in sys.modules; the three tiny collaborators are
faithful copies of the quarry code (mcp_config._strip_bearer_prefix /
_bearer_auth_headers/_env_key_for_server, config.redact_key -> agent.redact
mask_secret).  validate_mcp_server_entry is a no-issues seam: this gate tests
the create/redact contract, not the separate validation module.
"""
import importlib.util
import json
import re
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


_CTRL = re.compile(r"[\x00-\x1f\x7f\x80-\x9f\u200b-\u200f\u202a-\u202e\u2060-\u2064]")


def redact_key(value):
    """agent.redact.mask_secret(value, empty=color("(not set)", DIM))."""
    value = _CTRL.sub("", value) if value else value
    if not value:
        return "(not set)"
    return "***" if len(value) < 12 else value[:4] + "..." + value[-4:]


def _env_key_for_server(name):
    suffix = re.sub(r"[^A-Za-z0-9_]", "_", name.upper()).strip("_")
    return f"MCP_{suffix}_API_KEY"


def _bearer_auth_headers(name):
    return {"Authorization": f"Bearer ${{{_env_key_for_server(name)}}}"}


def _strip_bearer_prefix(token):
    if not isinstance(token, str):
        return token
    stripped = token.strip()
    if stripped[:7].lower() == "bearer ":
        return stripped[7:].strip()
    return stripped


hcli = _pkg("hermes_cli")
cfg = _pkg("hermes_cli.config")
cfg.redact_key = redact_key
hcli.config = cfg
wm = _pkg("hermes_cli.web_models")
wm.MCPServerCreate = type("MCPServerCreate", (), {})
hcli.web_models = wm
mc = _pkg("hermes_cli.mcp_config")
mc._strip_bearer_prefix = _strip_bearer_prefix
mc._bearer_auth_headers = _bearer_auth_headers
hcli.mcp_config = mc
sec = _pkg("hermes_cli.mcp_security")
sec.validate_mcp_server_entry = lambda name, cfg: []
hcli.mcp_security = sec

SRC = QUARRY + "/hermes_cli/web_server_mcp.py"
spec = importlib.util.spec_from_file_location("web_server_mcp_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["web_server_mcp_live"] = live
spec.loader.exec_module(live)


class Secret:
    def __init__(self, value):
        self.value = value

    def get_secret_value(self):
        return self.value


def compute(r):
    func = r.get("func")
    if func == "normalize_mcp_server_create":
        bt = r.get("bearer_token", None)
        body = types.SimpleNamespace(
            name=r.get("name"),
            url=r.get("url"),
            command=r.get("command"),
            auth=r.get("auth"),
            bearer_token=Secret(bt) if bt is not None else None,
            args=r.get("args"),
            env=r.get("env"),
        )
        try:
            name, cfg_out, tok = live.normalize_mcp_server_create(body)
        except ValueError as exc:
            return {"error": str(exc)}
        return {"name": name, "config": cfg_out, "token": tok}
    if func == "redact_mcp_env":
        return live._redact_mcp_env(r.get("env") or {})
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
        print(f"MISMATCH {r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_webserver_3794af: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
