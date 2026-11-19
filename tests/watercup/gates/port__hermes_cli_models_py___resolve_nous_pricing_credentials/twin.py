"""twin for hermes_cli/models.py:_resolve_nous_pricing_credentials (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "_resolve_nous_pricing_credentials"
    C_FN = "_resolve_nous_pricing_credentials"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
