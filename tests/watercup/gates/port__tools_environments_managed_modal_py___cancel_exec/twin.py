"""twin for tools/environments/managed_modal.py:_cancel_exec (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/managed_modal.py"
    PY_FN = "_cancel_exec"
    C_FN = "cli_tools_environments_managed_modal__cancel_exec"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
