"""twin for tools/file_operations.py:_is_write_denied (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/file_operations.py"
    PY_FN = "_is_write_denied"
    C_FN = "fo_is_write_denied"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
