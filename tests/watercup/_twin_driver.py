#!/usr/bin/env python3
"""_twin_driver.py — run one twin's input walk in an ISOLATED subprocess.

Why this exists
---------------
A watercup gate whose ``py_fn`` is a CLI/server entry point (``main``) executes
the REAL function. Several of those modules are process-level programs:
``gateway/run.py`` binds ports and runs a polling loop,
``agent/transports/hermes_tools_mcp_server.py`` and ``tools/mcp_death_supervisor.py``
spawn and supervise child processes, ``tools/neutts_synth.py`` spawns audio
subprocesses, ``hermes_cli/container_boot.py`` starts services.

Imported in-process by the runner, those crash or hang the HOST — observed as
repeated WSL deaths when a fleet run touched them. The runner therefore refuses
to import such twins in-process and delegates them here instead, so the worst a
gate can do is die in its own process, on its own timeout.

Contract
--------
argv: <twin.py path> <inputs.json path>
stdout: exactly one JSON object:
    {"cups": [...]}          success
    {"error": "message"}     any failure, including timeout-ish aborts
exit:  0 on success, non-zero only for a driver-level fault (bad argv)

The sandbox env (HERMES_HOME / SLERMES_HOME) must be set by the caller and is
inherited; this driver deliberately does NOT re-derive it, so the parent and
child agree on where operator state lands.
"""

import importlib.util
import json
import os
import sys


def _emit(obj):
    sys.stdout.write(json.dumps(obj))
    sys.stdout.flush()


def main(argv):
    if len(argv) != 3:
        _emit({"error": "usage: _twin_driver.py <twin.py> <inputs.json>"})
        return 2

    twin_path, inputs_path = argv[1], argv[2]
    try:
        with open(inputs_path) as f:
            inputs = json.load(f)
    except Exception as e:
        _emit({"error": "cannot read inputs: %s" % e})
        return 2

    HERE = os.path.dirname(os.path.abspath(__file__))
    REPO = os.path.dirname(os.path.dirname(HERE))

    # Mirror wc_run's import environment so the twin resolves the same modules.
    os.environ.setdefault("WATERCUP_NO_BROWSER", "1")
    sys.path.insert(0, HERE)
    sys.path.insert(0, os.path.dirname(REPO))  # parent quarry for LIVE imports

    # Same guards the in-process path installs. Best-effort: a guard that fails
    # to import must not mask the gate's own result.
    try:
        import browser_guard  # noqa: F401
    except Exception:
        pass
    try:
        import watercup_guard
        watercup_guard.install()
    except Exception:
        pass

    # argv hijack: an entry point calls argparse.parse_args() against argv.
    # Give it a bare argv so it sees no arguments rather than the driver's.
    sys.argv = [twin_path]

    # argparse reaches exit through ArgumentParser.exit (an instance method),
    # and SystemExit is a BaseException the twin's `except Exception` misses.
    # Neutralise both so the gate reports instead of killing this process.
    try:
        import argparse as _argparse

        def _exit_to_exception(self, status=0, message=None):
            raise SystemExit(status)

        _argparse.ArgumentParser.exit = _exit_to_exception
    except Exception:
        pass

    try:
        spec = importlib.util.spec_from_file_location("isolated_twin", twin_path)
        twin = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(twin)
        cups = twin.run(inputs)
        if isinstance(cups, str):
            cups = [cups] * len(inputs)
        _emit({"cups": list(cups)})
        return 0
    except SystemExit as e:
        _emit({"error": "SystemExit(%s)" % (e.code,)})
        return 1
    except BaseException as e:
        # Deliberately BaseException: anything the live code raises must come
        # back as data, never as this process's exit status.
        _emit({"error": "%s: %s" % (type(e).__name__, e)})
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
