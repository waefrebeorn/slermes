"""twin for agent/i18n.py:_load_catalog (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/i18n.py"
    PY_FN = "_load_catalog"
    C_FN = "_load_catalog"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
