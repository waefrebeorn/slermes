"""twin for gateway/platforms/qqbot/utils.py:build_user_agent (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/qqbot/utils.py"
    PY_FN = "build_user_agent"
    C_FN = "build_user_agent"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
