"""twin for gateway/display_config.py:resolve_display_setting (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/display_config.py"
    PY_FN = "resolve_display_setting"
    C_FN = "resolve_display_setting"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
