"""twin for agent/file_safety.py:is_write_denied (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/file_safety.py"
    PY_FN = "is_write_denied"
    C_FN = "fsf_is_write_denied"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
