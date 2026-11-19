"""twin for agent/markdown_tables.py:_pad_to_width (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/markdown_tables.py"
    PY_FN = "_pad_to_width"
    C_FN = "_pad_to_width"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
