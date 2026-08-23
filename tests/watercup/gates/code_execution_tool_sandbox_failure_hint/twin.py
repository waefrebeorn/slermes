"""twin for code_execution_tool_sandbox_failure_hint.

Imports LIVE tools/code_execution_tool.py and traces the SAME branch ladder
rung-order as the C harness (cet_sandbox_failure_hint traced copy), using the
module's own SANDBOX constants and regexes.
"""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_cet", os.path.join(DEV, "tools", "code_execution_tool.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_cet"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

BUILTIN_HELPERS = ["json_parse", "shell_quote", "retry"]


def run(inputs):
    cups = []
    for stderr_text in inputs:
        c = Cup("cet_sandbox_failure_hint")
        if not stderr_text:
            c.branch(True)                                   # B0
            c.valstr("<none>")
            cups.append(c.finalize())
            continue
        c.branch(False)
        window = stderr_text[:4000]
        c.valstr(window)

        m = re.search(r"cannot import name '(\w+)' from 'hermes_tools'", window)
        if not m:
            c.branch(False)                                  # B1
        else:
            c.branch(True)
            missing = m.group(1)
            c.valstr(missing)
            builtin = missing in BUILTIN_HELPERS
            # parity check with LIVE's builtin set
            assert builtin == (missing in {"json_parse", "shell_quote", "retry"})
            c.branch(builtin)                                # B2
            c.valstr("<none>")
            cups.append(c.finalize())
            continue

        m = re.search(r"NameError: name '(json_parse|shell_quote|retry)' is not defined", window)
        if not m:
            c.branch(False)                                  # B3
        else:
            c.branch(True)
            c.valstr(m.group(1))
            c.valstr("<none>")
            cups.append(c.finalize())
            continue

        m = re.search(r"ModuleNotFoundError: No module named '([\w.]+)'", window)
        if not m:
            c.branch(False)                                  # B4
        else:
            c.branch(True)
            c.valstr(m.group(1))
            c.valstr("<none>")
            cups.append(c.finalize())
            continue

        hit = bool(re.search(
            r"TypeError: string indices must be integers"
            r"|AttributeError: 'str' object has no attribute 'get'", window))
        c.branch(hit)                                        # B5
        c.valstr("<none>")
        cups.append(c.finalize())
    return cups
