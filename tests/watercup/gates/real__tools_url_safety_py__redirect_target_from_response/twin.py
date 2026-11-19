"""twin for tools/url_safety.py:redirect_target_from_response (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/url_safety.py"
    PY_FN = "redirect_target_from_response"
    C_FN = "redirect_target_from_response"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
