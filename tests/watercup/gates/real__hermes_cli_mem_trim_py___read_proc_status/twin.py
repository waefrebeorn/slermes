"""twin for hermes_cli/mem_trim.py:_read_proc_status (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/mem_trim.py"
    PY_FN = "_read_proc_status"
    C_FN = "_read_proc_status"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
