"""twin for gateway/platforms/base.py:get_inbound_media_max_bytes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "get_inbound_media_max_bytes"
    C_FN = "get_inbound_media_max_bytes"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
