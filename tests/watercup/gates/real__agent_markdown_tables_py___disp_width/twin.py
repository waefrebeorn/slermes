"""twin for agent/markdown_tables.py:_disp_width (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/markdown_tables.py"
    PY_FN = "_disp_width"
    C_FN = "mdt_disp_width"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
