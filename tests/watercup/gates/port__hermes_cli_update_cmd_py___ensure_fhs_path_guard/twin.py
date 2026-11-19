"""twin for hermes_cli/update_cmd.py:_ensure_fhs_path_guard (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_ensure_fhs_path_guard"
    C_FN = "uc_ensure_fhs_path_guard"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
