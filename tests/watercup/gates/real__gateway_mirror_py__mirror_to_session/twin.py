"""twin for gateway/mirror.py:mirror_to_session (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/mirror.py"
    PY_FN = "mirror_to_session"
    C_FN = "mirror_to_session"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
