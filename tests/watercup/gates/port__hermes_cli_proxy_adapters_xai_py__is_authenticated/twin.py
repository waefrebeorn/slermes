"""twin for hermes_cli/proxy/adapters/xai.py:is_authenticated (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/proxy/adapters/xai.py"
    PY_FN = "is_authenticated"
    C_FN = "pxa_is_authenticated"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
