"""twin for agent/gemini_schema.py:sanitize_gemini_schema (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/gemini_schema.py"
    PY_FN = "sanitize_gemini_schema"
    C_FN = "sanitize_gemini_schema"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
