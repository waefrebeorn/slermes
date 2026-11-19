"""twin for hermes_cli/model_catalog.py:_spawn_catalog_swr_refresh (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_catalog.py"
    PY_FN = "_spawn_catalog_swr_refresh"
    C_FN = "catalog_swr_refresh_thread"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
