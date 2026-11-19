"""twin for hermes_cli/profiles.py:_clone_all_copytree_ignore (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/profiles.py"
    PY_FN = "_clone_all_copytree_ignore"
    C_FN = "profile_clone_ignore"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
