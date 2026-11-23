#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_chat.py (w9_gatewaychat_13d2b8 gate).

os.environ, the shell client-env resolver and session-source creation are
fixture seams; the live module's own option/policy helpers run unmodified.
"""
import importlib.util
import json
import os
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


def _pkg(name):
    m = _mod(name)
    m.__path__ = []
    return m


cli = _pkg("hermes_cli")
_mod("hermes_cli.gateway_client",
     RESUME_KEY_MISMATCH="resume_key_mismatch",
     GatewayClientError=type("GatewayClientError", (Exception,), {}),
     connect_gateway=lambda: None)
_mod("hermes_cli.gateway_connection", RESUME_REFUSALS=frozenset())

# Real project truthiness semantics, loaded from the quarry.
uspec = importlib.util.spec_from_file_location("w9gc_utils_env", Q + "/utils/env.py")
umod = importlib.util.module_from_spec(uspec)
sys.modules["w9gc_utils_env"] = umod
uspec.loader.exec_module(umod)
_mod("utils", is_truthy_value=umod.is_truthy_value)
_mod("agent")
_mod("agent.session_source", create_surface=lambda s: "gui" if s == "desktop" else s)

SRC = Q + "/hermes_cli/gateway_chat.py"
spec = importlib.util.spec_from_file_location("w9gc_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w9gc_live"] = live
spec.loader.exec_module(live)

ENV_KEYS = ["HERMES_YOLO_MODE", "HERMES_IGNORE_RULES", "HERMES_SAFE_MODE",
            "HERMES_IGNORE_USER_CONFIG", "HERMES_ACCEPT_HOOKS"]


def apply_env(env):
    env = env or {}
    for k in ENV_KEYS:
        if k in env and env[k] is not None:
            os.environ[k] = str(env[k])
        else:
            os.environ.pop(k, None)


def ns(args):
    return types.SimpleNamespace(**(args or {}))


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
    saved = {k: os.environ.get(k) for k in ENV_KEYS}
    saved_shell = live.shell_client_env
    try:
        if func == "continue_title":
            py = live.continue_title(ns(r.get("args")))
        elif func == "wants_latest":
            py = live.wants_latest(ns(r.get("args")))
        elif func == "env_flags":
            apply_env(r.get("env"))
            py = live.env_launch_flags()
        elif func == "bypass":
            apply_env(r.get("env"))
            py = live.bypass_launch(ns(r.get("args")))
        elif func == "launch_flags":
            py = live._launch_flags(ns(r.get("args")))
        elif func == "requested_policy":
            apply_env(r.get("env"))
            py = live._requested_policy(ns(r.get("args")))
        elif func == "validate":
            apply_env(r.get("env"))
            try:
                live.validate_options(ns(r.get("args")))
                py = None
            except live.GatewayClientError as exc:
                py = str(exc)
        elif func == "resume_error":
            keys = r.get("keys") or []
            py = None if not keys else str(live.resume_policy_error(keys))
        elif func == "client_env_policy":
            live.shell_client_env = lambda: r.get("client_env")
            py = live.client_env_policy(r.get("parameters"))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    finally:
        for k, v in saved.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v
        live.shell_client_env = saved_shell
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w9_gatewaychat_13d2b8: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
