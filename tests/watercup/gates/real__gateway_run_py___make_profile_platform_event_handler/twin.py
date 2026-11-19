"""twin for gateway/run.py:_make_profile_platform_event_handler (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_make_profile_platform_event_handler"
    C_FN = "_make_profile_platform_event_handler"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
