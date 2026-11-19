"""twin for gateway/platforms/helpers.py:_chunk_newline_preferred (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/helpers.py"
    PY_FN = "_chunk_newline_preferred"
    C_FN = "_chunk_newline_preferred"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
