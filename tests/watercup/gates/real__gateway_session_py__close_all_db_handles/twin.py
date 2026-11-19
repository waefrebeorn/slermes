"""twin for gateway/session.py:close_all_db_handles (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/session.py"
    PY_FN = "close_all_db_handles"
    C_FN = "close_all_db_handles"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
