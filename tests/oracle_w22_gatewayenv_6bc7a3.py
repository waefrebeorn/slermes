#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/matrix/gateway_env.py
(gate w22_gatewayenv_6bc7a3).

The live declaration module is loaded by path, so ENV_OVERRIDES/AUTHZ/
ENV_ENABLE_CREDENTIALS and the real `_e2ee` tail step are what get compared.

  e2ee               -- env injected as os.environ for the duration of one call,
                        then restored; live `_e2ee(None, PlatformConfig())` renders
                        the resulting `extra`.
  authz              -- live AUTHZ dict.
  enable_credentials -- live ENV_ENABLE_CREDENTIALS tuple.
  env_overrides      -- the declared steps rendered as data: an EnvCred becomes
                        its dataclass fields, a functools.partial home_step its
                        bound keywords. Enums render as their .value, callables
                        by __name__, tuples/lists as lists.

Env values in the fixture are strings or null (null unsets), matching what
os.environ can hold; a non-object `env` carries no variables on either side.
"""
import functools
import importlib.util
import json
import os
import sys
import types
from enum import Enum
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "matrix" / "gateway_env.py"
GATE = "w22_gatewayenv_6bc7a3"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("matrix_gateway_env_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["matrix_gateway_env_live"] = live
_spec.loader.exec_module(live)

from gateway.config import PlatformConfig  # noqa: E402  (after the path insert)

WANTED = {"e2ee", "authz", "enable_credentials", "env_overrides"}
TOUCHED = ("MATRIX_E2EE_MODE", "MATRIX_ENCRYPTION", "MATRIX_DEVICE_ID")


def render(v):
    if v is None or isinstance(v, (bool, int, float, str)):
        return v
    if isinstance(v, Enum):
        return v.value
    if isinstance(v, (list, tuple)):
        return [render(x) for x in v]
    if callable(v):
        return getattr(v, "__name__", str(v))
    return str(v)


def e2ee(env):
    env = env if isinstance(env, dict) else {}
    saved = {name: os.environ.get(name) for name in TOUCHED}
    try:
        for name in TOUCHED:
            value = env.get(name)
            if isinstance(value, str):
                os.environ[name] = value
            else:
                os.environ.pop(name, None)
        matrix_config = PlatformConfig()
        live._e2ee(None, matrix_config)
        return dict(matrix_config.extra)
    finally:
        for name, value in saved.items():
            if value is None:
                os.environ.pop(name, None)
            else:
                os.environ[name] = value


def env_overrides():
    out = []
    for step in live.ENV_OVERRIDES:
        if isinstance(step, functools.partial):
            row = {"kind": "home_step"}
            for key, value in step.keywords.items():
                row[key] = render(value)
            out.append(row)
        else:
            row = {"kind": type(step).__name__}
            for key, value in vars(step).items():
                row[key] = render(value)
            out.append(row)
    return out


def expected(r):
    f = r.get("func")
    if f == "e2ee":
        return e2ee(r.get("env"))
    if f == "authz":
        return dict(live.AUTHZ)
    if f == "enable_credentials":
        return list(live.ENV_ENABLE_CREDENTIALS)
    if f == "env_overrides":
        return env_overrides()
    raise AssertionError("unknown func %r" % (f,))


def check_isolation():
    """The runner isolates HOME/SLERMES_HOME to one temp dir: assert it, don't assume."""
    home = os.environ.get("HOME")
    slermes = os.environ.get("SLERMES_HOME")
    same = bool(home and slermes and
                os.path.normcase(os.path.abspath(home)) ==
                os.path.normcase(os.path.abspath(slermes)))
    if not same or not os.path.isdir(home):
        print("HARNESS-ENV not isolated: HOME=%r SLERMES_HOME=%r" % (home, slermes),
              file=sys.stderr)
        sys.exit(2)


def main():
    check_isolation()
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
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
