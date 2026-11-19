"""twin for agent/secret_sources/onepassword.py:find_op (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/onepassword.py"
    PY_FN = "find_op"
    C_FN = "find_op"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
