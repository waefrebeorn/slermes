"""twin for gateway/acp.py:is_sensitive_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/acp.py"
    PY_FN = "is_sensitive_path"
    C_FN = "is_sensitive_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
