"""twin for hermes_cli/_subprocess_compat.py:_process_start_time (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_subprocess_compat.py"
    PY_FN = "_process_start_time"
    C_FN = "_process_start_time"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
