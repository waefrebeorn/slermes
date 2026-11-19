"""twin for gateway/platforms/base.py:_unwrap_ephemeral (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_unwrap_ephemeral"
    C_FN = "_unwrap_ephemeral"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
