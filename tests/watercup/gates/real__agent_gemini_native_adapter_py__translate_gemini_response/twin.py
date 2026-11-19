"""twin for agent/gemini_native_adapter.py:translate_gemini_response (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/gemini_native_adapter.py"
    PY_FN = "translate_gemini_response"
    C_FN = "translate_gemini_response"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
