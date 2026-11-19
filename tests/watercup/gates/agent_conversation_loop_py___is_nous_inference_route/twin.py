"""twin for agent/conversation_loop.py:_is_nous_inference_route (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_loop.py"
    PY_FN = "_is_nous_inference_route"
    C_FN = "cvl_is_nous_inference_route"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
