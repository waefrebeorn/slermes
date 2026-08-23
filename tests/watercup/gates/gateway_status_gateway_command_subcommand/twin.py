"""twin for gateway_status_gateway_command_subcommand.

Imports LIVE gateway/status.py::_gateway_command_subcommand and traces the
SAME rung ladder as the C harness (mirroring src/gateway/status.c).
Inputs are raw command lines (one per cup).
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_live_gwstatus",
                                   os.path.join(DEV, "gateway", "status.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_gwstatus"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("gateway_command_subcommand")
        result = None
        if not raw:
            c.branch(True)                       # B1
            c.valstr("")
            cups.append(c.finalize())
            continue
        c.branch(False)
        try:
            raw_tokens = mod.shlex.split(raw, posix=False)
        except ValueError:
            raw_tokens = raw.split()
        tokens = [t.strip("\"'").replace("\\", "/").lower() for t in raw_tokens]
        if not tokens:
            c.branch(True)                       # B2
            c.valstr("")
            cups.append(c.finalize())
            continue
        c.branch(0)

        done = False
        for token in tokens:
            if done:
                break
            if token == "gateway/run.py" or token.endswith("/gateway/run.py"):
                c.branch(True)                   # B3
                result = "run"
                done = True
            else:
                c.branch(False)
                basename = token.rsplit("/", 1)[-1]
                if basename in ("hermes-gateway", "hermes-gateway.exe"):
                    c.branch(True)               # B4
                    result = "run"
                    done = True
                else:
                    c.branch(False)

        if not done:
            joined = " ".join(tokens)
            has_entry = ("hermes_cli.main" in joined
                         or "hermes_cli/main.py" in joined)
            if not has_entry:
                for token in tokens:
                    if token.rsplit("/", 1)[-1] in ("hermes", "hermes.exe"):
                        has_entry = True
                        break
            if not has_entry:
                c.branch(True)                   # B7
                c.valstr("")
                cups.append(c.finalize())
                continue
            c.branch(False)

            filtered = []
            skip_next = False
            for token in tokens:
                if skip_next:
                    skip_next = False
                    c.branch(True)               # B8
                    continue
                c.branch(False)
                if token in ("--profile", "-p"):
                    skip_next = True
                    c.branch(True)               # B9
                    continue
                c.branch(False)
                if token.startswith("--profile=") or token.startswith("-p="):
                    c.branch(True)               # B10
                    continue
                c.branch(False)
                filtered.append(token)

            for i, token in enumerate(filtered):
                if token != "gateway":
                    c.branch(True)               # B11
                    continue
                c.branch(False)
                if i + 1 >= len(filtered):
                    c.branch(True)               # B12
                    result = "run"
                    break
                c.branch(False)
                result = filtered[i + 1]
                break

        c.valstr(result if result else "")
        cups.append(c.finalize())
    return cups
