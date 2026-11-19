"""twin for gateway/run.py:_collect_auto_append_media_tags (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_collect_auto_append_media_tags"
    C_FN = "_collect_auto_append_media_tags"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
