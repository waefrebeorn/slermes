"""twin for cli.py:_should_handle_steer_command_inline (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_should_handle_steer_command_inline"
    C_FN = "_should_handle_steer_command_inline"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
