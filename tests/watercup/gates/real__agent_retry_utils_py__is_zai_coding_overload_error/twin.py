"""twin for agent/retry_utils.py:is_zai_coding_overload_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/retry_utils.py"
    PY_FN = "is_zai_coding_overload_error"
    C_FN = "rtu_is_zai_coding_overload_error"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
