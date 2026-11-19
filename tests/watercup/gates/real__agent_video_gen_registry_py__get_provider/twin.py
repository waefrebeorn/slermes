"""twin for agent/video_gen_registry.py:get_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/video_gen_registry.py"
    PY_FN = "get_provider"
    C_FN = "get_provider"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
