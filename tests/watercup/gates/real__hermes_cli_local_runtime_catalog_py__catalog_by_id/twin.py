"""twin for hermes_cli/local_runtime/catalog.py:catalog_by_id (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/catalog.py"
    PY_FN = "catalog_by_id"
    C_FN = "catalog_by_id"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
