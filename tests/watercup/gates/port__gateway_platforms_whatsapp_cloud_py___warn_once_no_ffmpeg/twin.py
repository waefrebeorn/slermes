"""twin for gateway/platforms/whatsapp_cloud.py:_warn_once_no_ffmpeg (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/whatsapp_cloud.py"
    PY_FN = "_warn_once_no_ffmpeg"
    C_FN = "whatsapp_cloud_warn_once_no_ffmpeg"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
