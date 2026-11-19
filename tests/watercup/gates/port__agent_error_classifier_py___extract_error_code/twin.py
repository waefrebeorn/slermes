"""twin for agent/error_classifier.py:_extract_error_code (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/error_classifier.py"
    PY_FN = "_extract_error_code"
    C_FN = "agent_error_classifier_u_extract_error_code"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
