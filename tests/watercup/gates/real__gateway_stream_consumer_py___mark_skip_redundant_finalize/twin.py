"""twin for gateway/stream_consumer.py:_mark_skip_redundant_finalize (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/stream_consumer.py"
    PY_FN = "_mark_skip_redundant_finalize"
    C_FN = "_mark_skip_redundant_finalize"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
