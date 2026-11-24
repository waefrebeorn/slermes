#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/slack/gateway_env.py
(gate w22_gatewayenv_e84967).

The module's two side-effect seams are supplied here and nothing else is:
`getenv` reads the fixture env mapping, and the GatewayConfig / PlatformConfig /
HomeChannel / EnvCred objects are light stand-ins for the config package (which
is what the module imports at load time). The code under test -- the `_home`
function and the three module-level tables -- is lifted out of the quarry source
with an AST walk and executed as-is.

  home                   -- live `_home(config)`; the config it left behind is
                            read back.
  authz                  -- live AUTHZ dict.
  env_enable_credentials -- live ENV_ENABLE_CREDENTIALS tuple.
  env_overrides          -- live ENV_OVERRIDES steps, described as a cred step
                            or a plain step by name.
"""
import ast
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "slack" / "gateway_env.py"
GATE = "w22_gatewayenv_e84967"

WANTED = {"home", "authz", "env_enable_credentials", "env_overrides"}


# --- the config seam --------------------------------------------------------
class Platform:
    SLACK = "slack"


class HomeChannel:
    def __init__(self, platform, chat_id, name, thread_id=None, user_id=None, scope_id=None):
        self.platform = platform
        self.chat_id = chat_id
        self.name = name
        self.thread_id = thread_id
        self.user_id = user_id
        self.scope_id = scope_id


class PlatformConfig:
    def __init__(self, enabled=False):
        self.enabled = enabled
        self.home_channel = None


class GatewayConfig:
    pass


class EnvCred:
    def __init__(self, platform, creds, token=None, **kw):
        self.platform = platform
        self.creds = creds
        self.token = token


def fake_getenv_for(env):
    def getenv(name, default=""):
        value = env.get(name)
        return default if value is None else value
    return getenv


NS = {
    "Platform": Platform,
    "PlatformConfig": PlatformConfig,
    "HomeChannel": HomeChannel,
    "GatewayConfig": GatewayConfig,
    "EnvCred": EnvCred,
    "getenv": fake_getenv_for({}),
}

_tree = ast.parse(SRC.read_text(encoding="utf-8"))
_ASSIGN_NAMES = {"ENV_ENABLE_CREDENTIALS", "ENV_OVERRIDES", "AUTHZ"}
_found = set()
for _node in _tree.body:
    if isinstance(_node, ast.Assign):
        names = {t.id for t in _node.targets if isinstance(t, ast.Name)}
        if names & _ASSIGN_NAMES:
            exec(compile(ast.Module(body=[_node], type_ignores=[]), str(SRC), "exec"), NS)
            _found |= names & _ASSIGN_NAMES
    elif isinstance(_node, ast.FunctionDef) and _node.name == "_home":
        exec(compile(ast.Module(body=[_node], type_ignores=[]), str(SRC), "exec"), NS)
        _found.add("_home")
if _found != _ASSIGN_NAMES | {"_home"}:  # pragma: no cover
    raise SystemExit("gateway_env source changed; missing %r" % (sorted(_ASSIGN_NAMES | {"_home"} - _found),))

_home = NS["_home"]


class Cfg:
    def __init__(self, existing):
        self.platforms = {}
        if existing is not None:
            pc = PlatformConfig(enabled=bool(existing.get("enabled")))
            if existing.get("chat_id") is not None:
                pc.home_channel = HomeChannel(Platform.SLACK, existing.get("chat_id"),
                                              existing.get("name") or "",
                                              user_id=existing.get("user_id"),
                                              scope_id=existing.get("scope_id"))
            self.platforms[Platform.SLACK] = pc


def home(env, existing):
    NS["getenv"] = fake_getenv_for(env)
    cfg = Cfg(existing)
    _home(cfg)
    if Platform.SLACK not in cfg.platforms:
        return {"changed": False}
    pc = cfg.platforms[Platform.SLACK]
    hc = pc.home_channel
    return {
        "changed": True,
        "created": existing is None,
        "enabled": bool(pc.enabled),
        "chat_id": hc.chat_id,
        "name": hc.name,
        "thread_id": hc.thread_id,
        "user_id": hc.user_id,
        "scope_id": hc.scope_id,
    }


def describe(step):
    if isinstance(step, EnvCred):
        return {"kind": "cred", "platform": step.platform, "creds": list(step.creds),
                "token": step.token}
    return {"kind": "step", "name": getattr(step, "__name__", repr(step))}


def expected(r):
    f = r.get("func")
    if f == "home":
        return home(r.get("env") or {}, r.get("existing"))
    if f == "authz":
        return dict(NS["AUTHZ"])
    if f == "env_enable_credentials":
        return list(NS["ENV_ENABLE_CREDENTIALS"])
    if f == "env_overrides":
        return [describe(s) for s in NS["ENV_OVERRIDES"]]
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
