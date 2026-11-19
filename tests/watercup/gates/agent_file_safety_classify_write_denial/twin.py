"""twin for agent/file_safety.py:_classify_write_denial (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/file_safety.py"
    PY_FN = "_classify_write_denial"
    C_FN = "_classify_write_denial"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
