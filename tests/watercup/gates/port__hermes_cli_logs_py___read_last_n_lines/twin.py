"""twin for hermes_cli/logs.py:_read_last_n_lines (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/logs.py"
    PY_FN = "_read_last_n_lines"
    C_FN = "_read_last_n_lines"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
