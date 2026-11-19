"""twin for gateway/platforms/yuanbao_media.py:_basename_from_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao_media.py"
    PY_FN = "_basename_from_url"
    C_FN = "_basename_from_url"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
