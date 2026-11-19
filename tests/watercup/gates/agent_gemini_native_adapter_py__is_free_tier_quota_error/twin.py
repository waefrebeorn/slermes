"""twin for agent/gemini_native_adapter.py:is_free_tier_quota_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/gemini_native_adapter.py"
    PY_FN = "is_free_tier_quota_error"
    C_FN = "gna_is_free_tier_quota_error"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
