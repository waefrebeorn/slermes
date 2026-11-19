"""twin for tools/tirith_security.py:_load_security_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tirith_security.py"
    PY_FN = "_load_security_config"
    C_FN = "_load_security_config"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
