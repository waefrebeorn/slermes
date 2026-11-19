"""twin for agent/models_dev.py:_override_to_catalog_shape (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/models_dev.py"
    PY_FN = "_override_to_catalog_shape"
    C_FN = "_override_to_catalog_shape"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
