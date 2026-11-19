"""twin for gateway/run.py:_parse_reasoning_command_args (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_parse_reasoning_command_args"
    C_FN = "_parse_reasoning_command_args"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
