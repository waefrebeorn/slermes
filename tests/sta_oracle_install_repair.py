#!/usr/bin/env python3
"""sta_oracle_install_repair.py — Python twin for tests/t_port_install_repair.c
(hermes_cli/_install_repair.py). Imports the REAL module; side effects are
monkeypatched with fixture-driven fakes. One JSON output line per op.
"""
import contextlib
import io
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DEV = REPO.parent
sys.path.insert(0, str(DEV))

from hermes_cli import _install_repair as ir          # noqa: E402
from hermes_cli import _early_recovery as _er         # noqa: E402


def main():
    ops = json.load(open(sys.argv[1]))["ops"]
    for op in ops:
        kind = op.get("kind", "")
        if kind == "termux":
            env = op.get("env")
            print(json.dumps({"out": ir._is_termux_env(env)}))
        elif kind == "console_scripts":
            names = ir._load_console_script_names(Path(op["root"]))
            print(json.dumps({"out": [str(n) for n in names]}))
        elif kind == "extras":
            names = ir._load_installable_optional_extras(
                Path(op["root"]), op["group"])
            print(json.dumps({"out": list(names)}))
        elif kind == "bump":
            fd, path = tempfile.mkstemp(suffix=".json")
            with os.fdopen(fd, "w") as f:
                f.write(op.get("content", ""))
            try:
                attempts = ir.bump_marker_attempts(Path(path))
            finally:
                os.unlink(path)
            print(json.dumps({"out": attempts}))
        elif kind == "make_uv":
            home = Path(os.environ.get("HERMES_HOME") or os.environ.get("HOME"))
            d = home / op.get("at", ".hermes/bin")
            d.mkdir(parents=True, exist_ok=True)
            (d / "uv").write_text("#!/bin/sh\n")
            os.chmod(d / "uv", 0o755)
            print(json.dumps({"out": True}))
        elif kind == "resolve_target":
            real_exec = sys.executable
            sys.executable = op.get("exe", "python3")
            try:
                prefix, env = ir._resolve_install_target(Path(op.get("root", ".")))
                meta = {
                    "prefix": list(prefix),
                    "has_env": env is not None,
                    "virtual_env": str(env["VIRTUAL_ENV"]) if env else "",
                    "strip_python_vars": (
                        env is not None and ir._is_termux_env(env)),
                }
            finally:
                sys.executable = real_exec
            # strip_python_vars is only meaningful under Termux; under the
            # isolated env both sides see False. Normalize: when env exists,
            # the vars were popped iff termux was detected.
            print(json.dumps({"out": meta}))
        elif kind == "core_install":
            fail = {tuple(x) for x in op.get("fail", [])}
            uv = op.get("uv")

            def fake_find_uv():
                return uv or None

            real_find = _er._find_uv_binary
            _er._find_uv_binary = fake_find_uv

            commands = []

            class Done(Exception):
                pass

            def fake_run(cmd, cwd=None, check=False, capture_output=False, **kw):
                commands.append([str(c) for c in cmd])
                if tuple(str(c) for c in cmd) in fail:
                    raise subprocess.CalledProcessError(1, cmd)
                return subprocess.CompletedProcess(cmd, 0)

            real_sub_run = ir.subprocess.run
            ir.subprocess.run = fake_run
            real_exec = sys.executable
            sys.executable = op.get("exe", "python3")
            buf = io.StringIO()
            buf_err = io.StringIO()
            try:
                # run_core_install routes ALL prints through _stdout_to_stderr
                # (sys.stdout = sys.stderr for the duration), so capture stderr.
                with contextlib.redirect_stdout(buf), \
                     contextlib.redirect_stderr(buf_err):
                    ir.run_core_install(Path(op.get("root", ".")))
            finally:
                sys.executable = real_exec
                ir.subprocess.run = real_sub_run
                _er._find_uv_binary = real_find
            printed = [l for l in buf_err.getvalue().split("\n") if l != ""]
            print(json.dumps({"out": {"commands": commands,
                                      "printed": printed}}))
        else:
            print(json.dumps({"out": None}))


if __name__ == "__main__":
    main()
