"""twin for tools/clarify_gateway.py:_coerce_text_response_detailed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/clarify_gateway.py"
    PY_FN = "_coerce_text_response_detailed"
    C_FN = "_coerce_text_response_detailed"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
