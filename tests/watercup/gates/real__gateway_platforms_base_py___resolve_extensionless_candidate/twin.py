"""twin for gateway/platforms/base.py:_resolve_extensionless_candidate (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_resolve_extensionless_candidate"
    C_FN = "_resolve_extensionless_candidate"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
