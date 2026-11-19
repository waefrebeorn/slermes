"""twin for hermes_constants.py:_canonical_model_variants (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_constants.py"
    PY_FN = "_canonical_model_variants"
    C_FN = "_canonical_model_variants"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
