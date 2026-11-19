"""twin for gateway/platforms/qqbot/chunked_upload.py:upload (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/qqbot/chunked_upload.py"
    PY_FN = "upload"
    C_FN = "upload"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
