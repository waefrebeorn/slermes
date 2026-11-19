"""twin for gateway/platforms/base.py:_bounded_history_media_paths_for_session (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_bounded_history_media_paths_for_session"
    C_FN = "_bounded_history_media_paths_for_session"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
