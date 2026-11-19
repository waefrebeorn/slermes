"""twin for hermes_cli/proxy/adapters/xai.py:_load_pool (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/proxy/adapters/xai.py"
    PY_FN = "_load_pool"
    C_FN = "_load_pool"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
