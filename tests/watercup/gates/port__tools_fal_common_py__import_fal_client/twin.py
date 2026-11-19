"""twin for tools/fal_common.py:import_fal_client (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/fal_common.py"
    PY_FN = "import_fal_client"
    C_FN = "import_fal_client"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
