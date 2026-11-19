"""twin for agent/context_compressor.py:_load_anti_thrash_recovery_deadline (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_load_anti_thrash_recovery_deadline"
    C_FN = "_load_anti_thrash_recovery_deadline"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
