"""twin for tools/fal_common.py:_normalize_fal_queue_url_format (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/fal_common.py"
    PY_FN = "_normalize_fal_queue_url_format"
    C_FN = "_normalize_fal_queue_url_format"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
