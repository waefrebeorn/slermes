"""twin for gateway/stream_consumer.py:_flush_segment_tail_on_edit_failure (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/stream_consumer.py"
    PY_FN = "_flush_segment_tail_on_edit_failure"
    C_FN = "sc_flush_segment_tail_on_edit_failure"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
