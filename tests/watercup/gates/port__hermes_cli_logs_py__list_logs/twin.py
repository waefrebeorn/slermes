"""twin for hermes_cli/logs.py:list_logs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/logs.py"
    PY_FN = "list_logs"
    C_FN = "cli_logs_list"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
