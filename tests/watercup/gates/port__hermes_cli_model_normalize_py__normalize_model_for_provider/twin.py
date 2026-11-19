"""twin for hermes_cli/model_normalize.py:normalize_model_for_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_normalize.py"
    PY_FN = "normalize_model_for_provider"
    C_FN = "model_normalize_for_provider"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
