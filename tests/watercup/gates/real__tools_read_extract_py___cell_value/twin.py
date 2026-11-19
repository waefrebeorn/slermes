"""twin for tools/read_extract.py:_cell_value (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/read_extract.py"
    PY_FN = "_cell_value"
    C_FN = "_cell_value"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
