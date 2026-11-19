"""twin for cli.py:_clear_secret_input_buffer (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_clear_secret_input_buffer"
    C_FN = "_clear_secret_input_buffer"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
