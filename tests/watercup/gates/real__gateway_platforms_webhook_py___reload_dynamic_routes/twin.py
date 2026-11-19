"""twin for gateway/platforms/webhook.py:_reload_dynamic_routes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/webhook.py"
    PY_FN = "_reload_dynamic_routes"
    C_FN = "wh_reload_dynamic_routes"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
