"""twin for cron/blueprint_catalog.py:__post_init__ (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/blueprint_catalog.py"
    PY_FN = "__post_init__"
    C_FN = "__post_init__"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
