"""twin for cli.py:_preserve_ctrl_enter_newline (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_preserve_ctrl_enter_newline"
    C_FN = "_preserve_ctrl_enter_newline"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
