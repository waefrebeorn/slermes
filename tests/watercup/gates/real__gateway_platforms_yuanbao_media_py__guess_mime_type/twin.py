"""twin for gateway/platforms/yuanbao_media.py:guess_mime_type (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao_media.py"
    PY_FN = "guess_mime_type"
    C_FN = "guess_mime_type"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
