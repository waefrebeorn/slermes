"""twin for hermes_state.py:_raise_if_db_corrupt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_raise_if_db_corrupt"
    C_FN = "_raise_if_db_corrupt"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
