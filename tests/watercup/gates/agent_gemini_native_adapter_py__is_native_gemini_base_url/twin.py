"""twin for agent/gemini_native_adapter.py:is_native_gemini_base_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/gemini_native_adapter.py"
    PY_FN = "is_native_gemini_base_url"
    C_FN = "gna_is_native_gemini_base_url"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
