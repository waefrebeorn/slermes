"""twin for hermes_cli/local_runtime/catalog.py:_load_catalog (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/catalog.py"
    PY_FN = "_load_catalog"
    C_FN = "_load_catalog"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
