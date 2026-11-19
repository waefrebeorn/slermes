"""twin for agent/error_classifier.py:_extract_response_headers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/error_classifier.py"
    PY_FN = "_extract_response_headers"
    C_FN = "_extract_response_headers"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
