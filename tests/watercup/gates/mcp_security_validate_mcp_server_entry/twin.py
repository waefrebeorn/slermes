"""twin for mcp_security_validate_mcp_server_entry.

Imports LIVE hermes_cli/mcp_security.py::validate_mcp_server_entry and traces
the SAME rung ladder as the C harness. V event = issues joined with ";".
Inputs are "<name>|<entry_json>".
"""
import json as _json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_live_mcp_security",
                                   os.path.join(DEV, "hermes_cli", "mcp_security.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_mcp_security"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def run(inputs):
    cups = []
    fn = mod.validate_mcp_server_entry
    for raw in inputs:
        name, ej = raw.split("|", 1)
        try:
            entry = _json.loads(ej)
        except Exception:
            entry = None

        c = Cup("validate_mcp_server_entry")
        c.branch(isinstance(entry, dict))                    # B0
        if not isinstance(entry, dict):
            live = fn(name, entry)
            assert live == [], (raw, live)
            c.valstr("")
            cups.append(c.finalize())
            continue

        parts = [str(entry.get("command") or "")]
        args = entry.get("args")
        if isinstance(args, (list, tuple)):
            script = " ".join(str(a) for a in args)
        elif args is None:
            script = ""
        else:
            script = str(args)
        parts.append(script)
        env = entry.get("env")
        if isinstance(env, dict):
            parts.extend(str(v) for v in env.values())
        flat = " ".join(parts)

        ioc_hit = None
        for ioc in mod._IOC_SUBSTRINGS:
            c.loop()
            if ioc in flat:
                ioc_hit = ioc
                break
        c.branch(ioc_hit is not None)                        # B1

        if ioc_hit is not None:
            live = fn(name, entry)
            expect = ("MCP server '%s' contains a known hermes-0day "
                      "indicator-of-compromise ('%s')" % (name, ioc_hit))
            assert live == [expect], (raw, live)
            c.valstr(expect)
            cups.append(c.finalize())
            continue

        command = entry.get("command")
        text = str(command or "").strip()
        try:
            import shlex
            first = shlex.split(text)[0] if text else text
        except Exception:
            first = text.split()[0] if text.split() else text
        basename = os.path.basename(first).lower() if first else ""
        is_shell = basename in mod._SHELL_INTERPRETERS
        c.branch(is_shell)                                   # B2
        if not is_shell:
            live = fn(name, entry)
            assert live == [], (raw, live)
            c.valstr("")
            cups.append(c.finalize())
            continue

        c.branch(bool(script))                               # B3
        if not script:
            live = fn(name, entry)
            assert live == [], (raw, live)
            c.valstr("")
            cups.append(c.finalize())
            continue

        egress = bool(mod._EGRESS_PATTERN.search(script))
        c.branch(egress)                                     # B4
        hint = False
        if egress:
            hint = bool(mod._EXFIL_HINT_PATTERN.search(script))
            c.branch(hint)                                   # B5
        persist = bool(mod._PERSISTENCE_PATTERN.search(script))
        c.branch(persist)                                    # B6

        live = fn(name, entry)
        c.valstr(";".join(live))
        cups.append(c.finalize())
    return cups
