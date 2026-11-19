"""twin for hermes_cli/local_runtime/catalog.py:refresh_catalog_soon (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/catalog.py"
    PY_FN = "refresh_catalog_soon"
    C_FN = "refresh_catalog_soon"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
