"""twin for agent/image_routing.py:_sniff_mime_from_bytes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/image_routing.py"
    PY_FN = "_sniff_mime_from_bytes"
    C_FN = "_sniff_mime_from_bytes"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
