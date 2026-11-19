"""twin for agent/plugin_stream_hooks.py:_callback_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/plugin_stream_hooks.py"
    PY_FN = "_callback_name"
    C_FN = "_callback_name"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
