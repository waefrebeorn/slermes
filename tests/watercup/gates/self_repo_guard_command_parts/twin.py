"""twin for self_repo_guard_command_parts.

Imports LIVE tools/self_repo_guard.py::_command_parts and traces the SAME
rung ladder as the C harness. consume_options internals untraced on both
sides (shared helper).
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_live_srg",
                                   os.path.join(DEV, "tools", "self_repo_guard.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_srg"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def _canon(env, executable, args):
    envpart = ";".join("%s=%s" % (k, v) for k, v in env.items())
    if envpart:
        envpart += ";"
    argpart = ",".join(args)
    return "%s|%s|%s" % (envpart, executable or "", argpart)


def run(inputs):
    cups = []
    fn = mod._command_parts
    for raw in inputs:
        words = raw.split(" ") if raw else []
        c = Cup("srg_command_parts")
        env = {}
        index = 0
        result_val = None

        while index < len(words):
            import re
            if mod._ASSIGNMENT_RE.fullmatch(words[index]):
                c.branch(True)
                name, value = words[index].split("=", 1)
                env[name] = value
                index += 1
                continue
            c.branch(False)

            executable = mod._executable_name(words[index])
            if executable == "sudo":
                c.branch(True)
                index = mod._consume_options(words, index + 1,
                                             mod._SUDO_OPTIONS_WITH_ARG)
                continue
            c.branch(False)
            if executable == "env":
                c.branch(True)
                index = mod._consume_options(words, index + 1,
                                             mod._ENV_OPTIONS_WITH_ARG)
                continue
            c.branch(False)
            if executable == "command":
                c.branch(True)
                if index + 1 < len(words) and words[index + 1] in {"-v", "-V"}:
                    c.branch(True)
                    result_val = (env, None, [])
                    break
                c.branch(False)
                index = mod._consume_options(words, index + 1, frozenset())
                continue
            c.branch(False)
            if executable in mod._SIMPLE_WRAPPERS:
                c.branch(True)
                index = mod._consume_options(
                    words, index + 1,
                    mod._WRAPPER_OPTIONS_WITH_ARG.get(executable, frozenset()))
                continue
            c.branch(False)
            result_val = (env, words[index], words[index + 1:])
            break

        if result_val is None:
            result_val = (env, None, [])

        val = _canon(*result_val)
        assert _canon(*fn(words)) == val, (raw, fn(words), val)
        c.valstr(val)
        cups.append(c.finalize())
    return cups
