"""twin for hermes_cli/verify_cmd.py:_print_human_report (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/verify_cmd.py"
    PY_FN = "_print_human_report"
    C_FN = "verify_cmd_print_human_report"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
