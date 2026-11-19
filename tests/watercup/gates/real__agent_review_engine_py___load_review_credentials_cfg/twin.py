"""twin for agent/review_engine.py:_load_review_credentials_cfg (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/review_engine.py"
    PY_FN = "_load_review_credentials_cfg"
    C_FN = "_load_review_credentials_cfg"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
