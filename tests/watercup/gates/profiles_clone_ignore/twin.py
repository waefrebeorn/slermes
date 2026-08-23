"""twin for profiles_clone_ignore.

Imports LIVE hermes_cli/profiles.py::_clone_all_copytree_ignore and traces the
SAME branch ladder as the C harness for the inner _ignore(directory, [entry])
predicate: null entry -> universal exclusions -> at_root -> history excludes ->
is_default_source gate -> infra excludes -> result.

Inputs are "source_dir|dir|entry". HERMES_HOME is pinned to
/tmp/wcup_default_home so both sides agree on the default-profile root.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

os.environ["HERMES_HOME"] = "/tmp/wcup_default_home"

spec = ilu.spec_from_file_location("wc_profiles", os.path.join(DEV, "hermes_cli", "profiles.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_profiles"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

FN = "profile_clone_ignore"
UNIVERSAL_SUFFIXES = (".pyc", ".pyo", ".sock", ".tmp")


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup(FN)
        parts = (raw.split("|") + ["", "", ""])[:3]
        src, directory, entry = parts
        c.valstr(src)
        c.valstr(directory)
        c.valstr(entry)

        live_res = None
        if entry:
            ignore = mod._clone_all_copytree_ignore(__import__("pathlib").Path(src))
            ignored = ignore(directory, [entry])
            live_res = bool(ignored)

        result = False
        if not entry:
            c.branch(True)
        else:
            c.branch(False)
            universal = entry == "__pycache__" or entry.endswith(UNIVERSAL_SUFFIXES)
            c.branch(universal)
            if universal:
                result = True
            else:
                at_root = os.path.normpath(directory) == os.path.normpath(src)
                c.branch(at_root)
                if at_root:
                    hist = False
                    for name in ("state.db", "state.db-wal", "state.db-shm",
                                 "sessions", "backups", "state-snapshots", "checkpoints"):
                        c.loop()
                        if entry == name:
                            hist = True
                            break
                    c.branch(hist)
                    if hist:
                        result = True
                    else:
                        is_default = (directory == "/tmp/wcup_default_home")
                        c.branch(is_default)
                        if is_default:
                            infra = False
                            for name in ("hermes-agent", ".worktrees", "profiles",
                                         "bin", "node_modules"):
                                c.loop()
                                if entry == name:
                                    infra = True
                                    break
                            c.branch(infra)
                            if infra:
                                result = True
        assert result == live_res, "twin ladder diverged from LIVE (%r)" % (raw,)
        c.valstr("True" if live_res else "False")
        cups.append(c.finalize())
    return cups
