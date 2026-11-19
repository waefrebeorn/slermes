"""twin for agent/billing_links.py:is_nous_inference_route (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/billing_links.py"
    PY_FN = "is_nous_inference_route"
    C_FN = "is_nous_inference_route"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
