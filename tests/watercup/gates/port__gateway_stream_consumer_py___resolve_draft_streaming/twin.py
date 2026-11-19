"""twin for gateway/stream_consumer.py:_resolve_draft_streaming (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/stream_consumer.py"
    PY_FN = "_resolve_draft_streaming"
    C_FN = "sc_resolve_draft_streaming"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
