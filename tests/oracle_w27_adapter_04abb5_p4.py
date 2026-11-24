#!/usr/bin/env python3
"""Differential oracle for the Buzz adapter's configuration decisions and the
standalone sender (gate w27_adapter_04abb5_p4), driven against the LIVE
plugins/platforms/buzz/adapter.py.

Ported symbols and their live entry points:

  check_requirements   live.check_requirements()          (env-driven)
  validate_config      live.validate_config(config)       (env + extra)
  apply_yaml_config    live._apply_yaml_config({}, buzz)  (real _apply_yaml_bridge)
  env_enablement       live._env_enablement()             (real _seed_extra_from_env)
  standalone_send      live._standalone_send(...)         (seam: _exec_buzz)

Only two things are stubbed, both process/OS seams: the buzz subprocess
(_exec_buzz records argv/stdin and replays a scripted (rc, stdout, stderr)),
and the CLI binary existence probe (the fixture's "present" grows a real temp
file so _resolve_cli_path accepts it).  The env store is the input the C side
is handed directly.
"""
import asyncio
import json
import os
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w27_adapter_04abb5_p4"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_TMPHOME = tempfile.mkdtemp(prefix="w27-buzz-home-")
os.environ["HOME"] = _TMPHOME
os.environ["USERPROFILE"] = _TMPHOME

from plugins.platforms.buzz import adapter as live  # noqa: E402

_ORIGINAL_EXEC = live._exec_buzz

WANTED = {"check_requirements", "validate_config", "apply_yaml_config",
          "env_enablement", "standalone_send"}


def _clear_buzz_env():
    for key in [k for k in os.environ if k.startswith("BUZZ_")]:
        del os.environ[key]


def _fake_cli():
    path = Path(_TMPHOME) / "buzz-cli"
    if not path.exists():
        path.write_text("#!/bin/sh\n", encoding="utf-8")
    return str(path)


def check_requirements(r):
    _clear_buzz_env()
    os.environ["BUZZ_RELAY_URL"] = r.get("relay") or ""
    os.environ["BUZZ_PRIVATE_KEY"] = r.get("key") or ""
    return live.check_requirements()


def validate_config(r):
    _clear_buzz_env()
    os.environ["BUZZ_RELAY_URL"] = r.get("relay_env") or ""
    os.environ["BUZZ_PRIVATE_KEY"] = r.get("key") or ""
    cfg = type("Cfg", (), {"extra": dict(r.get("extra") or {})})()
    return live.validate_config(cfg)


def apply_yaml_config(r):
    snapshot = dict(os.environ)
    try:
        return live._apply_yaml_config({}, dict(r.get("buzz_cfg") or {}))
    finally:
        os.environ.clear()
        os.environ.update(snapshot)


def env_enablement(r):
    _clear_buzz_env()
    for key, value in (r.get("env") or {}).items():
        os.environ[key] = str(value)
    return live._env_enablement()


async def standalone_send(r):
    _clear_buzz_env()
    cli = _fake_cli() if (r.get("cli_path") or "") == "present" else ""
    os.environ["BUZZ_RELAY_URL"] = r.get("relay") or ""
    os.environ["BUZZ_PRIVATE_KEY"] = r.get("key") or ""
    os.environ["BUZZ_CLI_PATH"] = cli
    os.environ["BUZZ_HOME_CHANNEL"] = r.get("home_channel") or ""
    os.environ["BUZZ_REPLY_TO_MODE"] = r.get("reply_to_mode") or ""
    cfg = type("Cfg", (), {"extra": {}})()
    script = list(r.get("script") or [])
    calls = []
    state = {"i": 0}

    async def exec_buzz(cli_path, args, *, relay_url, private_key, auth_tag="",
                        input_text=None, timeout=None):
        calls.append({"args": list(args),
                      "input": "" if input_text is None else input_text,
                      "auth_tag": auth_tag or ""})
        if state["i"] >= len(script):
            raise RuntimeError("SCRIPT_EXHAUSTED")
        entry = script[state["i"]]
        state["i"] += 1
        return int(entry[0]), entry[1] or "", entry[2] or ""

    live._exec_buzz = exec_buzz
    try:
        out = await live._standalone_send(
            cfg, r.get("chat_id") or "", r.get("message") or "",
            thread_id=(r.get("thread_id") or None),
            media_files=(list(r.get("media_files") or []) or None))
    except RuntimeError:
        return {"calls": calls, "error": "SCRIPT_EXHAUSTED"}
    finally:
        live._exec_buzz = _ORIGINAL_EXEC
    return {"calls": calls, "result": out}


def expected(r):
    f = r.get("func")
    if f == "check_requirements":
        return check_requirements(r)
    if f == "validate_config":
        return validate_config(r)
    if f == "apply_yaml_config":
        return apply_yaml_config(r)
    if f == "env_enablement":
        return env_enablement(r)
    if f == "standalone_send":
        return asyncio.run(standalone_send(r))
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    if cases == 0:
        print("ORACLE %s: FAIL-CLOSED, no cases" % GATE, file=sys.stderr)
        sys.exit(2)
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
