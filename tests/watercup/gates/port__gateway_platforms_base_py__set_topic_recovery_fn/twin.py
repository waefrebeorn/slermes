"""twin for gateway/platforms/base.py:set_topic_recovery_fn (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "set_topic_recovery_fn"
    C_FN = "set_topic_recovery_fn"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
