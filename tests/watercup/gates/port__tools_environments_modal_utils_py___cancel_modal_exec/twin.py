"""twin for tools/environments/modal_utils.py:_cancel_modal_exec (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/modal_utils.py"
    PY_FN = "_cancel_modal_exec"
    C_FN = "cli_tools_environments_modal_utils__cancel_modal_exec"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
