"""twin for self_repo_guard_git_target_and_subcommand.

Imports LIVE tools/self_repo_guard.py::_git_target_and_subcommand and traces
the SAME branch ladder as the C harness (loop tick per iteration, arg-class
branches, explicit work-tree branch, subcommand branch, then valstr of
target / subcommand / remaining args / aliases).

Input grammar (parsed identically by twin and harness): cwd|envspec|argspec,
'-' = empty; envspec KEY=VALUE pairs joined by ';'; argspec args joined '+'.
Paths used in inputs must exist so LIVE Path.resolve() and C realpath() agree.
"""
import os, sys
from pathlib import Path
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, os.path.join(DEV, "tools"))
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_srg", os.path.join(DEV, "tools", "self_repo_guard.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_srg"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        cwd_s, envspec, argspec = (raw.split("|") + ["-", "-"])[:3]
        env = {}
        if envspec and envspec != "-":
            for pair in envspec.split(";"):
                if "=" in pair:
                    k, v = pair.split("=", 1)
                    env[k] = v
        args = [] if argspec in ("-", "") else argspec.split("+")
        c = Cup("git_target_and_subcommand")
        target = Path(cwd_s)
        work_tree = None
        aliases = {}
        index = 0
        while True:
            c.loop()
            if index >= len(args):
                c.branch(False)
                break
            arg = args[index]
            c.branch(True)
            if arg == "--":
                c.branch(True)
                index += 1
                break
            c.branch(False)
            is_C = (arg == "-C" and index + 1 < len(args))
            c.branch(is_C)
            if is_C:
                target = mod._resolve(args[index + 1], target)
                index += 2
                continue
            is_Cattached = (arg.startswith("-C") and len(arg) > 2)
            c.branch(is_Cattached)
            if is_Cattached:
                target = mod._resolve(arg[2:], target)
                index += 1
                continue
            is_wtflag = arg in {"--work-tree", "--git-dir", "--namespace", "--exec-path"}
            c.branch(is_wtflag)
            if is_wtflag:
                wt_val = (arg == "--work-tree" and index + 1 < len(args))
                c.branch(wt_val)
                if wt_val:
                    work_tree = args[index + 1]
                index += 2
                continue
            is_wteq = arg.startswith("--work-tree=")
            c.branch(is_wteq)
            if is_wteq:
                work_tree = arg.split("=", 1)[1]
                index += 1
                continue
            is_c = (arg == "-c" and index + 1 < len(args) and
                    args[index + 1].lower().startswith("alias.") and
                    "=" in args[index + 1])
            c.branch(is_c)
            if is_c:
                config = args[index + 1]
                key, value = config.split("=", 1)
                aliases[key[6:].lower()] = value
                index += 2
                continue
            is_calias = (arg.startswith("-calias.") and "=" in arg)
            c.branch(is_calias)
            if is_calias:
                key, value = arg[2:].split("=", 1)
                aliases[key[6:].lower()] = value
                index += 1
                continue
            is_dash = arg.startswith("-")
            c.branch(is_dash)
            if is_dash:
                index += 1
                continue
            break
        explicit_work_tree = work_tree or env.get("GIT_WORK_TREE")
        has_explicit = bool(explicit_work_tree)
        c.branch(has_explicit)
        if has_explicit:
            target = mod._resolve(explicit_work_tree, target)
        subcommand = args[index].lower() if index < len(args) else None
        has_sub = subcommand is not None
        c.branch(has_sub)
        c.valstr(str(target))
        c.valstr(subcommand or "")
        c.valstr("+".join(args[index + 1:]))
        c.valstr("+".join("%s=%s" % kv for kv in aliases.items()))
        cups.append(c.finalize())
    return cups
