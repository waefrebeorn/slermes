"""twin for tools/computer_use/vision_routing.py:_lookup_user_declared_supports_vision (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/vision_routing.py"
    PY_FN = "_lookup_user_declared_supports_vision"
    C_FN = "_lookup_user_declared_supports_vision"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
