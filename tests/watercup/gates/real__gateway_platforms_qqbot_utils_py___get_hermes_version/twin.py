"""twin for gateway/platforms/qqbot/utils.py:_get_hermes_version (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/qqbot/utils.py"
    PY_FN = "_get_hermes_version"
    C_FN = "_get_hermes_version"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
