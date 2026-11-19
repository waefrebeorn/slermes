"""twin for tools/read_extract.py:_workbook_sheets (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/read_extract.py"
    PY_FN = "_workbook_sheets"
    C_FN = "_workbook_sheets"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
