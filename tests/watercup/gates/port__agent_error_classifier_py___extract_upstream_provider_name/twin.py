"""twin for agent/error_classifier.py:_extract_upstream_provider_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/error_classifier.py"
    PY_FN = "_extract_upstream_provider_name"
    C_FN = "_extract_upstream_provider_name"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
