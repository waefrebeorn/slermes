"""twin for tools/browser_camofox_state.py:get_camofox_identity (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_camofox_state.py"
    PY_FN = "get_camofox_identity"
    C_FN = "get_camofox_identity"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
