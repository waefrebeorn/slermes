"""twin for gateway/session.py:_session_key_namespace (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/session.py"
    PY_FN = "_session_key_namespace"
    C_FN = "_session_key_namespace"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
