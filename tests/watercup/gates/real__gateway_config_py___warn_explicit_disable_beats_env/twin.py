"""twin for gateway/config.py:_warn_explicit_disable_beats_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/config.py"
    PY_FN = "_warn_explicit_disable_beats_env"
    C_FN = "_warn_explicit_disable_beats_env"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
