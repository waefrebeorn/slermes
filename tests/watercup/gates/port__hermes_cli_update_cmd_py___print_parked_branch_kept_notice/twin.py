"""twin for hermes_cli/update_cmd.py:_print_parked_branch_kept_notice (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_print_parked_branch_kept_notice"
    C_FN = "upd_print_parked_branch_kept_notice"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
