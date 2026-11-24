#!/usr/bin/env python3
"""Differential oracle for tools/terminal_tool_sudo_askpass.py (gate
w23_terminalto_b54e19).

The prologue is a shell program: ``build_prologue`` fills its placeholders, and
``fill_timeout_token`` rewrites the one token ``BaseEnvironment.execute`` cannot
know at prepare time.  Both run live and are compared as text, byte for byte.

``_option_words_carry_own_input`` is the pure option reader behind
``has_own_stdin_or_askpass`` (the counted-sudo rewrite it sits on top of is a
separate port); it runs live over the fixture's raw words, shlex.split included.

``sudo_askpass_supported_once`` reaches the backend through seams only, so the
fixture scripts the backend: its capability flags, its ``_run_bash`` /
``_wait_for_process`` pair, and ``platform.system``.  The answer, whether the
probe ran, and the timeout it was handed are what the backend would observe.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "tools" / "terminal_tool_sudo_askpass.py"
GATE = "w23_terminalto_b54e19"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("sudo_askpass_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["sudo_askpass_live"] = live
_spec.loader.exec_module(live)

WANTED = {"prologue", "fill", "own_input", "supported", "consts"}


def consts(_raw):
    return {
        "token": live.SUDO_ASKPASS_TIMEOUT_TOKEN,
        "prefix": live.SUDO_ASKPASS_PREFIX,
        "suffix": live.SUDO_ASKPASS_SUFFIX,
        "probe": live.CAPABILITY_PROBE,
    }


def prologue(r):
    token = r["timeout_token"] if "timeout_token" in r else live.SUDO_ASKPASS_TIMEOUT_TOKEN
    words = tuple(r["sudo_words"]) if "sudo_words" in r else ("sudo",)
    return live.build_prologue(r.get("count", 0), token,
                               background=bool(r.get("background", True)),
                               sudo_words=words)


def fill(r):
    return live.fill_timeout_token(r.get("command"), r.get("timeout"))


def own_input(r):
    words = r.get("words")
    return bool(live._option_words_carry_own_input(list(words if isinstance(words, list) else [])))


class Backend:
    """The seams sudo_askpass_supported_once reads."""

    def __init__(self, world):
        self._world = world
        self.runs = []
        if "cached" in world and world["cached"] is not None:
            self._sudo_askpass_ok = world["cached"]
        if world.get("runtime_checked"):
            self._sudo_askpass_runtime_checked = True
        if world.get("probe_supported"):
            self._passwordless_sudo_probe_supported = True
        if world.get("is_local"):
            self.is_local = True
        if "probe_timeout" in world:
            self._SUDO_PROBE_TIMEOUT_S = world["probe_timeout"]

    def _run_bash(self, command, timeout=None):
        self.runs.append((command, timeout))
        if self._world.get("run_raises"):
            raise OSError("probe backend is gone")
        return {"command": command, "timeout": timeout}

    def _wait_for_process(self, proc, timeout=None):
        return {"returncode": self._world.get("run_rc", 0)}


def supported(raw):
    world = raw if isinstance(raw, dict) else {}
    env = Backend(world)
    saved = live.platform
    live.platform = types.SimpleNamespace(system=lambda: world.get("system", "Linux"))
    try:
        answer = live.sudo_askpass_supported_once(env)
    finally:
        live.platform = saved
    return {
        "supported": bool(answer),
        "runs": len(env.runs),
        "timeout": env.runs[0][1] if env.runs else None,
        "cached_after": getattr(env, "_sudo_askpass_ok", None),
    }


def expected(r):
    f = r.get("func")
    if f == "prologue":
        return prologue(r)
    if f == "fill":
        return fill(r)
    if f == "own_input":
        return own_input(r)
    if f == "supported":
        return supported(r.get("world"))
    if f == "consts":
        return consts(r)
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
