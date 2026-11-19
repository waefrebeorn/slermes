"""twin for gateway/stream_consumer.py:_metadata_for_send (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/stream_consumer.py"
    PY_FN = "_metadata_for_send"
    C_FN = "_metadata_for_send"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
