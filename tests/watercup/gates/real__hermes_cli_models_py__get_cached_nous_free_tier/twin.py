"""twin for hermes_cli/models.py:get_cached_nous_free_tier (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "get_cached_nous_free_tier"
    C_FN = "get_cached_nous_free_tier"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
