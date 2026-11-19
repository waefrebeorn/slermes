"""twin for agent/curator.py:_extract_absorbed_into_declarations (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/curator.py"
    PY_FN = "_extract_absorbed_into_declarations"
    C_FN = "_extract_absorbed_into_declarations"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
