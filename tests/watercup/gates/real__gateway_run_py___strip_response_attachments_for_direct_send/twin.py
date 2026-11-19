"""twin for gateway/run.py:_strip_response_attachments_for_direct_send (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_strip_response_attachments_for_direct_send"
    C_FN = "gw_strip_response_attachments"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
