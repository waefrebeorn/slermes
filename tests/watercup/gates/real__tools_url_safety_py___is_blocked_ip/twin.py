"""twin for tools/url_safety.py:_is_blocked_ip (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/url_safety.py"
    PY_FN = "_is_blocked_ip"
    C_FN = "floor"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
