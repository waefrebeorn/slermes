"""twin for hermes_cli/profiles.py:_is_wrapper_dir_in_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/profiles.py"
    PY_FN = "_is_wrapper_dir_in_path"
    C_FN = "profile_wrapper_dir_in_path"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
