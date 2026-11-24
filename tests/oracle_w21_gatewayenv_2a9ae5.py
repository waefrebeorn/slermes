#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/qqbot/gateway_env.py
(gate w21_gatewayenv_2a9ae5).

The live module's own source runs: `_home` is lifted as a FunctionDef and
called against the fixture's environment, with the real `Platform.QQBOT` and
`HomeChannel` types; `AUTHZ`, `ENV_ENABLE_CREDENTIALS` and `ENV_OVERRIDES` are
the module's real assignments, so the EnvCred row is a live EnvCred instance.
Only `getenv` is a seam (the environment value under test) and the logger
records whether the deprecation warning fired.
"""
import ast
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "qqbot" / "gateway_env.py"
GATE = "w21_gatewayenv_2a9ae5"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from gateway.config import HomeChannel, Platform, PlatformConfig  # noqa: E402
from gateway.platform_env import EnvCred  # noqa: E402

ENV = {}


def getenv(name, default=None):
    """gateway.platform_env.getenv: os.environ text, or the default if absent."""
    if default is None:
        default = ""
    return ENV[name] if name in ENV else default


class Logger:
    def __init__(self):
        self.records = []

    def warning(self, message, *args, **kwargs):
        self.records.append(message)


LOGGER = Logger()

G = {"__name__": "qq_gateway_env_helpers", "getenv": getenv, "logger": LOGGER,
     "Platform": Platform, "HomeChannel": HomeChannel, "EnvCred": EnvCred,
     "GatewayConfig": object, "PlatformConfig": PlatformConfig}
WANT_FUNCS = {"_home"}
WANT_ASSIGNS = {"AUTHZ", "ENV_ENABLE_CREDENTIALS", "ENV_OVERRIDES"}

_tree = ast.parse(SRC.read_text(encoding="utf-8"))
for _node in _tree.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in WANT_FUNCS:
        exec(compile(ast.Module(body=[_node], type_ignores=[]), str(SRC), "exec"), G)
for _node in _tree.body:
    if isinstance(_node, (ast.Assign, ast.AnnAssign)):
        _targets = _node.targets if isinstance(_node, ast.Assign) else [_node.target]
        if any(isinstance(t, ast.Name) and t.id in WANT_ASSIGNS for t in _targets):
            exec(compile(ast.Module(body=[_node], type_ignores=[]), str(SRC), "exec"), G)

missing = (WANT_FUNCS | WANT_ASSIGNS) - set(G)
if missing:
    raise SystemExit("oracle setup failed, missing %r" % (sorted(missing),))


def canon(value):
    if isinstance(value, tuple):
        return [canon(v) for v in value]
    if isinstance(value, list):
        return [canon(v) for v in value]
    if isinstance(value, dict):
        return {k: canon(v) for k, v in value.items()}
    if isinstance(value, Platform):
        return value.value
    return value


def expected(r):
    global ENV
    func = r.get("func")
    if func == "home":
        ENV = r.get("env") or {}
        del LOGGER.records[:]
        config = PlatformConfig()
        G["_home"](None, config)
        home = config.home_channel
        return {
            "name_env": "QQ_HOME_CHANNEL_NAME" if LOGGER.records else "QQBOT_HOME_CHANNEL_NAME",
            "warned": bool(LOGGER.records),
            "home_channel": None if home is None else {
                "platform": canon(home.platform),
                "chat_id": home.chat_id,
                "name": home.name,
                "thread_id": home.thread_id,
            },
        }
    if func == "authz":
        return canon(G["AUTHZ"])
    if func == "env_enable":
        return list(G["ENV_ENABLE_CREDENTIALS"])
    if func == "plan":
        row = G["ENV_OVERRIDES"][0]
        if not isinstance(row, EnvCred):
            raise AssertionError("ENV_OVERRIDES[0] is not an EnvCred")
        return {
            "platform": canon(row.platform),
            "creds": [list(group) for group in row.creds],
            "token": row.token,
            "fixed": [list(item) for item in row.fixed],
            "optional": [list(item) for item in row.optional],
            "optional_stripped": [list(item) for item in row.optional_stripped],
            "warn_missing": list(row.warn_missing) if row.warn_missing else None,
            "then": row.then.__name__ if row.then is not None else None,
            "home": row.home,
            "home_strip": row.home_strip,
        }
    raise AssertionError("unknown func %r" % (func,))


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
        if not isinstance(r, dict) or r.get("func") not in {"home", "authz", "env_enable", "plan"}:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001 - a live raise is a harness bug
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
