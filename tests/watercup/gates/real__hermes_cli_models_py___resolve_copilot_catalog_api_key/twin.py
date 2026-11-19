"""twin for hermes_cli/models.py:_resolve_copilot_catalog_api_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "_resolve_copilot_catalog_api_key"
    C_FN = "_resolve_copilot_catalog_api_key"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
