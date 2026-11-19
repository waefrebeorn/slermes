"""twin for hermes_cli/update_cmd.py:_print_update_completion (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_print_update_completion"
    C_FN = "upd_print_update_completion"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
