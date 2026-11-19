"""twin for gateway/platforms/yuanbao_media.py:generate_file_id (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao_media.py"
    PY_FN = "generate_file_id"
    C_FN = "generate_file_id"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
