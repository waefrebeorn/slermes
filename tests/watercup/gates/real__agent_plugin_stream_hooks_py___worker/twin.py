"""twin for agent/plugin_stream_hooks.py:_worker (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/plugin_stream_hooks.py"
    PY_FN = "_worker"
    C_FN = "_worker"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
