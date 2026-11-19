"""twin for tools/code_execution_tool.py:_spill_full_stdout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/code_execution_tool.py"
    PY_FN = "_spill_full_stdout"
    C_FN = "_spill_full_stdout"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
