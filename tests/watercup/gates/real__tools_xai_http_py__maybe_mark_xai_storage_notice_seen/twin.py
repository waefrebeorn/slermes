"""twin for tools/xai_http.py:maybe_mark_xai_storage_notice_seen (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/xai_http.py"
    PY_FN = "maybe_mark_xai_storage_notice_seen"
    C_FN = "maybe_mark_xai_storage_notice_seen"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
