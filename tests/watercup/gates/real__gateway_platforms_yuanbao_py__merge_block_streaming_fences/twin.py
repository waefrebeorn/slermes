"""twin for gateway/platforms/yuanbao.py:merge_block_streaming_fences (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao.py"
    PY_FN = "merge_block_streaming_fences"
    C_FN = "merge_block_streaming_fences"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
