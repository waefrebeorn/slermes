"""twin for tools/read_extract.py:_zip_xml (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/read_extract.py"
    PY_FN = "_zip_xml"
    C_FN = "_zip_xml"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
