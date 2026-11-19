"""twin for hermes_cli/fallback_config.py:get_fallback_chain (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/fallback_config.py"
    PY_FN = "get_fallback_chain"
    C_FN = "get_fallback_chain"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
